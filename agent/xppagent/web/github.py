"""GitHub as the dashboard sees it: one GraphQL query for the whole page,
and REST calls for the owner's actions. It uses the owner's own token, which
the agent and its Claude runs never see."""
from __future__ import annotations

import json
import re
import urllib.error
import urllib.parse
import urllib.request

API = "https://api.github.com"

OVERVIEW = """
query($owner: String!, $name: String!) {
  viewer { login }
  repository(owner: $owner, name: $name) {
    issues(states: OPEN, first: 60, orderBy: {field: UPDATED_AT, direction: DESC}) {
      nodes {
        number title url createdAt updatedAt body
        author { login }
        labels(first: 12) { nodes { name } }
        milestone { title }
      }
    }
    pullRequests(states: OPEN, first: 30, orderBy: {field: UPDATED_AT, direction: DESC}) {
      nodes {
        number title url isDraft createdAt updatedAt mergeable headRefName
        additions deletions changedFiles body
        author { login }
        commits(last: 1) { nodes { commit { oid } } }
      }
    }
    merged: pullRequests(states: MERGED, first: 40, orderBy: {field: UPDATED_AT, direction: DESC}) {
      nodes { number title url mergedAt author { login } mergedBy { login }
              mergeCommit { oid } }
    }
    milestones(states: OPEN, first: 20, orderBy: {field: NUMBER, direction: ASC}) {
      nodes { number title description url progressPercentage }
    }
    roadmap: object(expression: "HEAD:ROADMAP.md") { ... on Blob { text } }
    roadmapHistory: defaultBranchRef {
      target { ... on Commit {
        history(first: 40, path: "ROADMAP.md") {
          nodes { oid committedDate messageHeadline url
                  parents(first: 1) { nodes { oid } }
                  associatedPullRequests(first: 1) { nodes { number title url } } }
        }
      } }
    }
  }
}
"""


class GitHubAPIError(RuntimeError):
    pass


class GitHubAPI:
    def __init__(self, slug: str, token: str, timeout: float = 25):
        self.slug = slug
        self.owner, self.name = slug.split("/", 1)
        self.token = token
        self.timeout = timeout

    def _request(self, method: str, path: str, body=None):
        url = path if path.startswith("https://") else API + path
        data = None if body is None else json.dumps(body).encode("utf-8")
        req = urllib.request.Request(url, data=data, method=method, headers={
            "Authorization": f"Bearer {self.token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
            "User-Agent": "xpplab-dashboard"})
        if data is not None:
            req.add_header("Content-Type", "application/json")
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                raw = r.read()
        except urllib.error.HTTPError as e:
            detail = e.read().decode("utf-8", "replace")
            try:
                detail = json.loads(detail).get("message", detail)
            except (ValueError, AttributeError):
                pass
            raise GitHubAPIError(f"GitHub said {e.code}: {str(detail)[:300]}") from None
        except (urllib.error.URLError, TimeoutError, OSError) as e:
            raise GitHubAPIError(f"GitHub could not be reached: {e}") from None
        return json.loads(raw) if raw else None

    # -- reading ---------------------------------------------------------------

    def overview(self) -> dict:
        """The page's data. Parts the token may not read come back empty
        rather than failing the whole page; `warnings` says which."""
        out = self._request("POST", "/graphql", {
            "query": OVERVIEW, "variables": {"owner": self.owner, "name": self.name}})
        errors = out.get("errors") or []
        if errors and not (out.get("data") or {}).get("repository"):
            raise GitHubAPIError("; ".join(e.get("message", "?") for e in errors)[:400])
        data = out["data"]
        data["warnings"] = sorted({e.get("message", "?") for e in errors})
        return data

    def roadmap_texts(self, oids: list[str]) -> dict[str, str]:
        """ROADMAP.md as it was at each of these commits, in one query ("" where
        the file did not exist yet)."""
        oids = [o for o in oids if re.fullmatch(r"[0-9a-f]{40}", o or "")]
        if not oids:
            return {}
        parts = " ".join(f'r{i}: object(expression: "{oid}:ROADMAP.md") {{ ... on Blob {{ text }} }}'
                         for i, oid in enumerate(oids))
        query = ("query($owner: String!, $name: String!) { repository(owner: $owner, "
                 f"name: $name) {{ {parts} }} }}")
        out = self._request("POST", "/graphql", {
            "query": query, "variables": {"owner": self.owner, "name": self.name}})
        repo = ((out or {}).get("data") or {}).get("repository") or {}
        return {oid: (repo.get(f"r{i}") or {}).get("text") or "" for i, oid in enumerate(oids)}

    def workflow_jobs(self, sha: str) -> list[dict]:
        """CI for a commit: the jobs of its newest run of each workflow.
        (Fine-grained tokens cannot read check runs; Actions they can.)"""
        runs = self._request("GET", f"/repos/{self.slug}/actions/runs?head_sha={sha}&per_page=30")
        newest: dict = {}
        for run in (runs or {}).get("workflow_runs", []):
            key = run.get("workflow_id") or run.get("name")
            if key not in newest or run.get("run_number", 0) > newest[key].get("run_number", 0):
                newest[key] = run
        jobs = []
        for run in newest.values():
            out = self._request("GET", f"/repos/{self.slug}/actions/runs/{run['id']}/jobs?per_page=50")
            for j in (out or {}).get("jobs", []):
                jobs.append({"name": j.get("name"), "status": j.get("status"),
                             "conclusion": j.get("conclusion"), "url": j.get("html_url")})
        return jobs

    def comments(self, number: int) -> list[dict]:
        return self._request("GET", f"/repos/{self.slug}/issues/{number}/comments?per_page=100")

    def pr_files(self, number: int) -> list[dict]:
        return self._request("GET", f"/repos/{self.slug}/pulls/{number}/files?per_page=100")

    # -- the owner's actions ---------------------------------------------------

    def merge(self, pr: int) -> dict:
        return self._request("PUT", f"/repos/{self.slug}/pulls/{pr}/merge",
                             {"merge_method": "rebase"})

    def comment(self, number: int, body: str) -> dict:
        return self._request("POST", f"/repos/{self.slug}/issues/{number}/comments",
                             {"body": body})

    def add_labels(self, number: int, labels: list[str]) -> None:
        self._request("POST", f"/repos/{self.slug}/issues/{number}/labels",
                      {"labels": labels})

    def remove_label(self, number: int, label: str) -> None:
        try:
            self._request("DELETE", f"/repos/{self.slug}/issues/{number}/labels/"
                                    + urllib.parse.quote(label, safe=""))
        except GitHubAPIError as e:
            if "404" not in str(e):
                raise

    def create_issue(self, title: str, body: str, labels: list[str]) -> int:
        out = self._request("POST", f"/repos/{self.slug}/issues",
                            {"title": title, "body": body, "labels": labels})
        return out["number"]

    def close(self, number: int) -> None:
        self._request("PATCH", f"/repos/{self.slug}/issues/{number}", {"state": "closed"})

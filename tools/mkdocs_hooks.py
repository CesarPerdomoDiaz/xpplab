"""MkDocs hooks: make the in-app manual pages read well on the web.

The desktop app's manual uses two special link targets:
    [Open the model](example:lecar.ode)   opens an example in the app
    [Run it](cmd:run)                      runs a command in the app
On the website the first becomes a link to the model file on GitHub, and
the second becomes the command shown as code.

Each page also ends with a link that opens a feedback issue about it, and
the reference pages rank first in a search. Both are added here, not in
the pages, which the app's Help window shows as they are.
"""

import re
from urllib.parse import urlencode

REPO = "https://github.com/CesarPerdomoDiaz/xpplab/blob/main/engine/xppaut/ode/"
ISSUES = "https://github.com/CesarPerdomoDiaz/xpplab/issues/new"

_example = re.compile(r"\[([^\]]+)\]\(example:([^)]+)\)")
_command = re.compile(r"\[([^\]]+)\]\(cmd:([^)]+)\)")


def feedback_link(page, config):
    """The address of a feedback issue about this page, its title and the
    page's address filled in (the feedback form's field is `feedback`)."""
    title = page.title or page.file.src_uri
    url = config.get("site_url", "") + page.url
    query = urlencode({"template": "feedback.yml", "title": f"Docs: {title}",
                       "feedback": f"About {url}\n\n"})
    return f"{ISSUES}?{query}"


def on_page_markdown(markdown, page, config, files):
    # A search for a command finds its reference page first (#125). Set
    # here, not in the page's front matter, which the app's Help window
    # would show as text.
    if page.file.src_uri.startswith("reference/"):
        page.meta.setdefault("search", {}).setdefault("boost", 2)
    markdown = _example.sub(lambda m: f"[{m.group(1)}]({REPO}{m.group(2)})",
                            markdown)
    markdown = _command.sub(lambda m: f"{m.group(1)} (`{m.group(2)}`)",
                            markdown)
    # "Was this page helpful?" without tracking anyone (#125): a link that
    # opens the feedback form about this page.
    markdown = markdown.rstrip() + (
        "\n\n---\n\n"
        "*Something wrong, unclear or missing on this page? "
        f"[Tell us]({feedback_link(page, config)}).*\n")
    return markdown

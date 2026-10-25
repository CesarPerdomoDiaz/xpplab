// Mission control for The Limit Cyclist: polls /api/overview, follows the
// Claude run in progress through /api/live, and sends the owner's actions.
"use strict";

const $ = (id) => document.getElementById(id);
const STAGES = [
  ["queued", "Waiting to start"],
  ["planned", "Building"],
  ["pr", "Agent review"],
  ["ci", "CI"],
  ["owner", "Your review"],
  ["blocked", "Needs your answer"],
];
const STAGE_NAME = Object.fromEntries(STAGES.concat([
  ["merged", "Merged"], ["closed", "Closed"], ["failed", "Failed"]]));
const KIND_NAME = {
  plan: "Planning", implement: "Building", review: "Reviewing", gates: "Fixing the checks",
  "review-fix": "Fixing review findings", fix: "Fixing", "ci-fix": "Fixing CI", owner: "Answering you",
  answer: "Using your answer", propose: "Proposing work", explain: "Explaining",
};

let page = null;          // the last overview
let live = { file: null, after: 0, timer: null };
const drafts = new Map(); // text typed in the page survives redraws
const opened = new Map(); // and so does which sections are open (key -> bool)
const replies = new Set(); // and which reply boxes are open

// -- small helpers -------------------------------------------------------------

function esc(s) {
  return String(s ?? "").replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
}

function ago(t) {
  if (!t) return "";
  const s = Math.max(0, Date.now() / 1000 - (typeof t === "string" ? Date.parse(t) / 1000 : t));
  if (s < 60) return "just now";
  if (s < 3600) return `${Math.round(s / 60)} min ago`;
  if (s < 86400) return `${Math.round(s / 3600)} h ago`;
  return `${Math.round(s / 86400)} d ago`;
}

function took(seconds) {
  if (seconds == null) return "";
  if (seconds < 90) return `${Math.round(seconds)} s`;
  return `${Math.round(seconds / 60)} min`;
}

function clock(t) {
  return new Date(t * 1000).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

// Markdown, safely: everything is escaped first, then a few constructs are
// turned back into tags. Links must be http(s).
function md(text) {
  const blocks = [];
  let s = esc(text || "").replace(/```[a-z]*\n?([\s\S]*?)```/g, (_, code) => {
    blocks.push(`<pre><code>${code}</code></pre>`);
    return `\u0000${blocks.length - 1}\u0000`;
  });
  s = s.replace(/`([^`\n]+)`/g, "<code>$1</code>")
    .replace(/\*\*([^*\n]+)\*\*/g, "<strong>$1</strong>")
    .replace(/(^|\s)_([^_\n]+)_(?=\s|[.,;:]|$)/g, "$1<em>$2</em>")
    .replace(/\[([^\]\n]+)\]\((https?:\/\/[^)\s]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>');
  const out = [];
  let list = null;
  for (const line of s.split("\n")) {
    const bullet = line.match(/^\s*[-*]\s+(.*)$/);
    const head = line.match(/^#{1,6}\s+(.*)$/);
    if (bullet) {
      if (!list) { list = []; }
      list.push(`<li>${bullet[1]}</li>`);
      continue;
    }
    if (list) { out.push(`<ul>${list.join("")}</ul>`); list = null; }
    if (head) out.push(`<p><strong>${head[1]}</strong></p>`);
    else if (line.trim()) out.push(`<p>${line}</p>`);
  }
  if (list) out.push(`<ul>${list.join("")}</ul>`);
  return out.join("").replace(/\u0000(\d+)\u0000/g, (_, i) => blocks[+i]);
}

function toast(message, bad = false) {
  const t = $("toast");
  t.textContent = message;
  t.className = "show" + (bad ? " bad" : "");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => (t.className = ""), bad ? 6000 : 3500);
}

async function api(path, options = {}) {
  const r = await fetch(path, { credentials: "same-origin", ...options });
  if (r.status === 401) { location.href = "/login"; throw new Error("log in first"); }
  const data = await r.json().catch(() => ({}));
  if (!r.ok) throw new Error(data.error || `HTTP ${r.status}`);
  return data;
}

async function act(action, data = {}, button = null) {
  if (page && page.read_only) { toast("This dashboard is read-only.", true); return; }
  if (button) button.disabled = true;
  try {
    const out = await api("/api/action", {
      method: "POST",
      headers: { "Content-Type": "application/json", "X-CSRF": page ? page.csrf : "" },
      body: JSON.stringify({ action, ...data }),
    });
    toast(out.message || "Done");
    setTimeout(refresh, 1500);
    return out;
  } catch (e) {
    toast(e.message, true);
  } finally {
    if (button) button.disabled = false;
  }
}

// -- drawing ---------------------------------------------------------------------

function drawTop(p) {
  const a = p.agent;
  if (p.bot) {
    $("avatar").src = `https://github.com/${encodeURIComponent(p.bot)}.png?size=88`;
  }
  $("repo").textContent = `XPP Lab's maintainer agent · ${p.repo}`;
  const status = $("status");
  let kind = "idle", text = "Idle, checking for work every minute";
  if (!a.alive) {
    kind = "down"; text = a.seen ? `Not responding, last seen ${ago(a.seen)}` : "Not started yet";
  } else if (a.paused) {
    kind = "paused"; text = a.stop_file ? "Stopped (STOP file)" : "Paused";
  } else if (a.current) {
    const c = a.current;
    kind = "working";
    text = `${KIND_NAME[c.kind] || c.kind}${c.issue ? " #" + c.issue : ""} · ${took(Date.now() / 1000 - c.started)}`;
  } else if (/limit|budget|quiet|signed out|holding back/i.test(a.doing || "")) {
    kind = "waiting"; text = `Waiting: ${a.doing}`;
  }
  if (a.alive && a.explaining) {
    if (kind === "idle") { kind = "working"; text = `Explaining #${a.explaining.issue}`; }
    else text += ` · explaining #${a.explaining.issue}`;
  }
  status.className = `pill ${kind}`;
  $("status-text").textContent = text;
  const used = a.runs_used ?? 0, max = a.runs_per_window || 1;
  $("budget-text").textContent = `Claude runs: ${used} of ${max} in ${a.window_hours} h`;
  $("budget-bar").style.width = `${Math.min(100, (100 * used) / max)}%`;
  // The subscription's own use (yours and the bot's), as the last run reported it.
  const share = [], u = a.usage || {}, nowS = Date.now() / 1000;
  for (const [key, name, limit] of [["five_hour", "5 h", a.max_five_hour_percent],
                                    ["seven_day", "week", a.max_week_percent]]) {
    const w = u[key];
    if (w && w.resets_at > nowS) share.push(`${name} ${Math.round(100 * w.utilization)}% (bot stops at ${limit}%)`);
  }
  $("share-text").textContent = share.length ? `Subscription: ${share.join(" · ")}` : "";
  const pause = $("pause");
  pause.textContent = a.paused && !a.stop_file ? "Resume" : "Pause";
  pause.disabled = p.read_only || a.stop_file;
  const banner = $("banner");
  const notes = [];
  if (p.read_only) notes.push("Read-only: actions are turned off.");
  if (p.github_error) notes.push(`GitHub: ${p.github_error}`);
  banner.textContent = notes.join("  ");
  banner.classList.toggle("hidden", !notes.length);
}

function checksHtml(checks) {
  if (!checks || !checks.items.length) return '<span class="chip">no CI for this change</span>';
  return checks.items.map((c) => {
    const r = (c.result || "pending").replace(/\s/g, "_");
    const mark = r === "success" ? "✓" : r === "skipped" || r === "neutral" ? "–" : /fail|error|cancel|timed/.test(r) ? "✕" : "…";
    return `<span class="chip ${esc(r)}" title="${esc(c.result)}">${mark} ${esc(c.name)}</span>`;
  }).join("");
}

// What the bot said, or is saying, about issue or pull request n, and a box
// to ask it something. `where` keeps the ids apart: the inbox and the board
// may both show the same issue.
function explainHtml(n, ex, where) {
  const id = `x${where}${n}`;
  let shown = "";
  if (ex) {
    const asked = ex.question ? `You asked: “${esc(ex.question.length > 160 ? ex.question.slice(0, 160) + "…" : ex.question)}”`
      : "You asked for a short brief";
    if (ex.status === "answered") {
      shown = `<details open data-key="${id}-answer" class="explained"><summary>The bot explains · ${asked}</summary>
        <div class="md">${md(ex.answer)}</div></details>`;
    } else if (ex.status === "failed") {
      shown = `<p class="explained notok">${asked}. The bot could not answer: ${esc(ex.note)}</p>`;
    } else {
      const doing = {
        sent: "Sent; the bot picks it up within a minute", asked: "The bot has it and starts soon",
        working: "The bot is reading the code to answer", waiting: `Waiting: ${esc(ex.note)}`,
      }[ex.status] || esc(ex.status);
      shown = `<p class="explained muted">${asked} ${ago(ex.asked)}. ${doing}…</p>`;
    }
  }
  return `${shown}<div class="reply" id="reply-${id}">
      <textarea data-key="${id}" placeholder="What should the bot explain? Leave it empty for a short brief: the problem, the plan, the risks, and whether to accept it."></textarea>
      <div class="row"><button class="primary" data-explain="${n}" data-key="${id}">Ask the bot</button></div>
    </div>`;
}

function drawInbox(p) {
  const box = $("inbox");
  const items = p.inbox;
  $("inbox-count").textContent = items.length;
  $("inbox-count").classList.toggle("hidden", !items.length);
  if (!items.length) {
    box.innerHTML = '<p class="empty">Nothing waits for you. The bot will ask here when it needs you.</p>';
    return;
  }
  box.innerHTML = items.map((it) => {
    if (it.kind === "review") {
      const ready = it.mergeable === "MERGEABLE";
      return `<div class="item review">
        <h3><a href="${esc(it.url)}" target="_blank" rel="noopener">#${it.pr}</a> ${esc(it.title)}</h3>
        <div class="meta">Ready for your review · for issue #${it.issue} · ${it.files} file${it.files === 1 ? "" : "s"},
          <span class="ok">+${it.additions}</span> <span class="notok">−${it.deletions}</span>
          ${ready ? "" : ` · <span class="notok">${esc((it.mergeable || "").toLowerCase())}</span>`}</div>
        <div class="md">${md(it.summary)}</div>
        <div class="checks">${checksHtml(it.checks)}</div>
        ${it.review ? `<details data-key="review-${it.pr}"><summary>The agent review</summary><div class="md">${md(it.review)}</div></details>` : ""}
        <div class="row">
          <button class="good" data-merge="${it.pr}" ${ready ? "" : "disabled"}>Merge</button>
          <button data-open="r${it.pr}">Ask for changes…</button>
          <button data-open="xi${it.pr}">Explain…</button>
          <a class="button" href="${esc(it.url)}/files" target="_blank" rel="noopener">See the diff</a>
          <button class="danger" data-close="${it.pr}">Close without merging</button>
        </div>
        <div class="reply" id="reply-r${it.pr}">
          <textarea data-key="r${it.pr}" placeholder="What should the bot change? It reads this and pushes an update."></textarea>
          <div class="row"><button class="primary" data-send="${it.pr}" data-key="r${it.pr}">Send to the bot</button></div>
        </div>
        ${explainHtml(it.pr, it.explain, "i")}
      </div>`;
    }
    if (it.kind === "question") {
      return `<div class="item question">
        <h3><a href="${esc(it.url)}" target="_blank" rel="noopener">#${it.issue}</a> ${esc(it.title)}</h3>
        <div class="meta">The bot is stuck and asks:</div>
        <div class="md">${md(it.question)}</div>
        <div class="reply open">
          <textarea data-key="q${it.issue}" placeholder="Your answer. The bot carries on as soon as it reads it."></textarea>
          <div class="row"><button class="primary" data-send="${it.issue}" data-key="q${it.issue}">Answer</button>
            <button data-open="xi${it.issue}">Explain first…</button></div>
        </div>
        ${explainHtml(it.issue, it.explain, "i")}
      </div>`;
    }
    if (it.kind === "issue") {
      return `<div class="item issue">
        <h3><a href="${esc(it.url)}" target="_blank" rel="noopener">#${it.issue}</a> ${esc(it.title)}</h3>
        <div class="meta">Opened by ${esc(it.author || "someone")} ${ago(it.created)}. The bot leaves it alone until you approve it.</div>
        <details data-key="issue-${it.issue}"><summary>What they wrote</summary><div class="md">${md(it.body)}</div></details>
        <div class="row">
          <button data-open="xi${it.issue}">Explain…</button>
          <button class="good" data-approve="${it.issue}">Approve: let the bot fix it</button>
          <button class="danger" data-close-issue="${it.issue}">Close</button>
        </div>
        ${explainHtml(it.issue, it.explain, "i")}
      </div>`;
    }
    return `<div class="item proposal">
      <h3><a href="${esc(it.url)}" target="_blank" rel="noopener">#${it.issue}</a> ${esc(it.title)}</h3>
      <div class="meta">The bot proposes this${it.milestone ? ` for ${esc(it.milestone)}` : ""}</div>
      <details open data-key="proposal-${it.issue}"><summary>Its proposal</summary><div class="md">${md(it.body)}</div></details>
      <div class="row">
        <button class="good" data-approve="${it.issue}">Approve: do it</button>
        <button data-open="xi${it.issue}">Explain…</button>
        <button class="danger" data-decline="${it.issue}">Decline</button>
      </div>
      ${explainHtml(it.issue, it.explain, "i")}
    </div>`;
  }).join("");
}

function drawBoard(p) {
  // The whole pipeline as a strip of counts; cards only where there is work.
  const recent = p.tasks.filter((t) => ["merged", "closed", "failed"].includes(t.stage)).slice(0, 6);
  const strip = STAGES.map(([stage, name]) => {
    const n = p.tasks.filter((t) => t.stage === stage).length;
    return `<span class="step${n ? " busy" : ""}">${name}<b>${n}</b></span>`;
  }).join('<span class="arrow">›</span>');
  const lanes = STAGES.map(([stage, name]) => {
    const cards = p.tasks.filter((t) => t.stage === stage);
    return cards.length ? `<div class="lane"><h3>${name}</h3>${cards.map(ticket).join("")}</div>` : "";
  }).filter(Boolean);
  if (recent.length) lanes.push(`<div class="lane"><h3>Done lately</h3>${recent.map(ticket).join("")}</div>`);
  $("strip").innerHTML = strip;
  $("board").innerHTML = lanes.join("") || '<p class="empty">No task in progress. Give the bot one with "Ask the bot".</p>';

  function ticket(t) {
    const issue = `https://github.com/${p.repo}/issues/${t.issue}`;
    const pr = t.pr ? ` · <a href="https://github.com/${p.repo}/pull/${t.pr}" target="_blank" rel="noopener">PR #${t.pr}</a>` : "";
    const since = t.stage === "ci" && t.waiting_since ? t.waiting_since : t.updated;
    return `<div class="ticket"><a href="${issue}" target="_blank" rel="noopener"><b>#${t.issue}</b></a> ${esc(t.title)}
      <div class="age">${STAGE_NAME[t.stage] || t.stage}${pr} · ${ago(since)} ·
        <button class="link" data-open="xt${t.issue}">explain</button></div>
      ${explainHtml(t.issue, t.explain, "t")}</div>`;
  }
}

function drawRuns(p) {
  $("runs").innerHTML = p.runs.map((r) => `<tr class="click" data-run="${esc(r.events || r.name + ".jsonl")}" data-label="${esc((KIND_NAME[r.kind] || r.kind) + (r.issue ? " #" + r.issue : ""))}">
      <td class="when">${r.started ? clock(r.started) : ""}<br>${ago(r.started)}</td>
      <td>${r.issue ? `#${r.issue}` : ""}</td>
      <td>${esc(KIND_NAME[r.kind] || r.kind || "")}</td>
      <td>${took(r.seconds)}</td>
      <td>${r.ok ? '<span class="ok">ok</span>' : '<span class="notok">failed</span>'}${r.turns ? ` <span class="muted">· ${r.turns} turns</span>` : ""}</td>
    </tr>`).join("") || '<tr><td colspan="5" class="empty">No runs yet.</td></tr>';
}

function drawSide(p) {
  $("events").innerHTML = p.events.map(([t, text]) =>
    `<li><span class="when">${clock(t)} · ${ago(t)}</span><br>${esc(text)}</li>`).join("")
    || '<li class="empty">Nothing yet.</li>';
  $("prs").innerHTML = p.prs.map((r) => `<li><a href="${esc(r.url)}" target="_blank" rel="noopener">#${r.number}</a>
      ${esc(r.title)} <span class="muted small">· ${esc(r.author || "")}${r.draft ? " · draft" : ""}</span>
      <div class="checks">${checksHtml(r.checks)}</div></li>`).join("")
    || '<li class="empty">None open.</li>';
  const roadmap = p.roadmap.map((m) => {
    const pct = m.total ? Math.round((100 * m.done) / m.total) : 0;
    const open = m.items.filter((i) => !i.done);
    return `<details data-key="m-${esc(m.id)}" class="milestone${m.total && m.done === m.total ? " complete" : ""}">
      <summary><div class="line"><span><b>${esc(m.id)}</b> ${esc(m.title.replace(/:\s*done$/i, ""))}</span>
        <span class="muted">${m.done}/${m.total}</span></div>
        <div class="bar"><span data-pct="${pct}"></span></div></summary>
      ${open.length ? `<ul>${open.map((i) => `<li>${esc(i.text)}</li>`).join("")}</ul>` : '<p class="small muted">All done.</p>'}
    </details>`;
  }).join("");
  $("roadmap").innerHTML = roadmap || '<p class="empty">ROADMAP.md was not found.</p>';
  $("roadmap").querySelectorAll("[data-pct]").forEach((s) => (s.style.width = `${s.dataset.pct}%`));
  $("footer").textContent = `Updated ${new Date().toLocaleTimeString()}` +
    (p.github_age != null ? ` · GitHub data from ${p.github_age} s ago` : "");
}

// -- the run in progress ---------------------------------------------------------

function feedItem(e) {
  const tag = e.kind === "tool" ? e.tool : { say: "says", start: "start", end: e.ok ? "done" : "failed", error: "error" }[e.kind] || e.kind;
  const text = e.kind === "end" ? `${e.text || ""}${e.turns ? ` (${e.turns} turns, ${took(e.seconds)})` : ""}` : e.text;
  return `<li class="${esc(e.kind)}"><span class="tag">${esc(tag)}</span><span class="txt">${esc(text)}</span></li>`;
}

async function followLive() {
  // the main loop's run, or else an explanation being written
  const c = page && (page.agent.current || page.agent.explaining);
  const feed = $("feed");
  if (!c) {
    if (live.file) { live = { file: null, after: 0, timer: live.timer }; }
    const waiting = page ? page.tasks.filter((t) => ["ci", "owner", "blocked"].includes(t.stage)) : [];
    $("now-meta").textContent = "";
    $("now-text").innerHTML = page && !page.agent.alive
      ? '<span class="notok">The agent is not running.</span> The workstation starts it again within 5 minutes.'
      : waiting.length
        ? "No Claude run in progress. Waiting for: " + waiting.map((t) => `${STAGE_NAME[t.stage].toLowerCase()} on #${t.issue}`).join(", ") + "."
        : "No Claude run in progress.";
    if (!feed.dataset.keep) feed.innerHTML = "";
    return;
  }
  if (live.file !== c.file) {
    live.file = c.file; live.after = 0; feed.innerHTML = ""; feed.dataset.keep = "";
  }
  $("now-text").textContent = `${KIND_NAME[c.kind] || c.kind}${c.issue ? " issue #" + c.issue : ""}${c.read_only ? " (read only)" : ""}`;
  $("now-meta").textContent = `started ${clock(c.started)} · ${took(Date.now() / 1000 - c.started)}`;
  try {
    const out = await api(`/api/live?file=${encodeURIComponent(c.file)}&after=${live.after}`);
    live.after = out.after;
    if (out.events.length) {
      const stick = feed.scrollTop + feed.clientHeight >= feed.scrollHeight - 30;
      feed.insertAdjacentHTML("beforeend", out.events.map(feedItem).join(""));
      while (feed.children.length > 400) feed.firstElementChild.remove();
      if (stick) feed.scrollTop = feed.scrollHeight;
    }
  } catch (e) { /* the next poll tries again */ }
}

async function showRun(file, label) {
  $("run-title").textContent = label;
  $("run-feed").innerHTML = '<li><span class="tag">…</span><span class="txt">Loading</span></li>';
  $("run-dialog").showModal();
  try {
    const out = await api(`/api/live?file=${encodeURIComponent(file)}&after=0`);
    $("run-feed").innerHTML = out.events.map(feedItem).join("") || '<li class="empty">No events were recorded for this run.</li>';
  } catch (e) {
    $("run-feed").innerHTML = `<li class="error"><span class="tag">error</span><span class="txt">${esc(e.message)}</span></li>`;
  }
}

// -- events -----------------------------------------------------------------------

document.addEventListener("click", async (ev) => {
  const b = ev.target.closest("button, tr[data-run]");
  if (!b) return;
  const d = b.dataset;
  if (d.run) return showRun(d.run, d.label);
  if (d.merge && confirm(`Merge pull request #${d.merge} into main (rebase)?`)) {
    await act("merge", { pr: +d.merge }, b);
  } else if (d.open) {
    const r = $(`reply-${d.open}`);
    if (r.classList.toggle("open")) { replies.add(d.open); r.querySelector("textarea").focus(); }
    else replies.delete(d.open);
  } else if (d.send) {
    const box = document.querySelector(`textarea[data-key="${d.key}"]`);
    const body = box.value.trim();
    if (!body) return toast("Write something first.", true);
    if (await act("comment", { number: +d.send, body }, b)) { box.value = ""; drafts.delete(d.key); replies.delete(d.key); }
  } else if (d.explain) {
    // an empty question asks for a short brief
    const box = document.querySelector(`textarea[data-key="${d.key}"]`);
    if (await act("explain", { number: +d.explain, question: box.value.trim() }, b)) {
      box.value = ""; drafts.delete(d.key); replies.delete(d.key);
      box.closest(".reply").classList.remove("open");
    }
  } else if (d.close && confirm(`Close #${d.close} without merging? The bot stops working on it.`)) {
    await act("close", { number: +d.close }, b);
  } else if (d.closeIssue && confirm(`Close issue #${d.closeIssue}?`)) {
    await act("close", { number: +d.closeIssue }, b);
  } else if (d.approve) {
    await act("approve", { issue: +d.approve }, b);
  } else if (d.decline) {
    const reason = prompt("Why not? (optional; the bot learns from it)");
    if (reason !== null) await act("decline", { issue: +d.decline, reason }, b);
  } else if (b.id === "pause") {
    await act(page.agent.paused ? "resume" : "pause", {}, b);
  } else if (b.id === "run-close") {
    $("run-dialog").close();
  }
});

$("ask").addEventListener("submit", async (ev) => {
  ev.preventDefault();
  const f = ev.target;
  const out = await act("ask", { title: f.title.value, body: f.body.value, priority: f.priority.checked },
                        f.querySelector("button"));
  if (out) f.reset();
});

// -- the path: the roadmap's main path as a graph ------------------------------
// Each milestone is a hub on the path, left to right in the order ROADMAP.md's
// "Path:" line gives; side tracks hang below. Its items circle it, filled when
// done; the merges that ticked them sit just outside. The strip below is the
// last merges in order: green moved the path, grey ticked nothing.

const SVGNS = "http://www.w3.org/2000/svg";
let evoKey = "";      // the data last drawn: redraw only when it changes
let evoView = null;   // the viewBox, kept across redraws
let evoBox = null;    // the drawing's extent, for Fit
let evoNodes = [];    // what a click can select: {el, info}

function svgEl(tag, attrs, parent) {
  const e = document.createElementNS(SVGNS, tag);
  for (const [k, v] of Object.entries(attrs || {})) e.setAttribute(k, v);
  if (parent) parent.appendChild(e);
  return e;
}

function svgText(parent, x, y, cls, text) {
  const t = svgEl("text", { x, y, class: cls }, parent);
  t.textContent = text;
  return t;
}

function shortTitle(t) {
  return t.replace(/\s*\(.*?\)/g, "").replace(/:.*$/, "").replace(/`/g, "").trim();
}

// Two lines of at most n characters, cut at words.
function twoLines(text, n) {
  const words = text.split(/\s+/);
  const lines = [""];
  for (const w of words) {
    const cur = lines[lines.length - 1];
    if ((cur + " " + w).trim().length > n && cur) {
      if (lines.length === 2) { lines[1] = lines[1].slice(0, n - 1) + "…"; break; }
      lines.push(w);
    } else lines[lines.length - 1] = (cur + " " + w).trim();
  }
  return lines;
}

function day(iso) {
  return iso ? new Date(iso).toLocaleDateString([], { day: "numeric", month: "short" }) : "";
}

function setView(v) {
  evoView = v;
  $("evo").setAttribute("viewBox", `${v.x} ${v.y} ${v.w} ${v.h}`);
}

function fitView() {
  if (!evoBox) return;
  const svg = $("evo");
  const W = svg.clientWidth || 1000, H = svg.clientHeight || 480, pad = 30;
  let w = evoBox.w + 2 * pad, h = evoBox.h + 2 * pad;
  if (w / h < W / H) w = (h * W) / H; else h = (w * H) / W;
  setView({ x: evoBox.x + evoBox.w / 2 - w / 2, y: evoBox.y + evoBox.h / 2 - h / 2, w, h });
}

function evoInfo(html) {
  const box = $("evo-info");
  box.innerHTML = html ? `<button class="link evo-close" type="button">Close</button>${html}` : "";
  box.classList.toggle("hidden", !html);
}

function stepLinks(steps) {
  return steps.length ? `<ul class="plain small">${steps.map((s) =>
    `<li><a href="${esc(s.url)}" target="_blank" rel="noopener">${s.pr ? `#${s.pr}` : esc(s.oid.slice(0, 7))}</a>
      ${esc(s.title)} <span class="muted">· ${esc(day(s.date))}</span></li>`).join("")}</ul>`
    : '<p class="small muted">No merge in the recent history ticked it.</p>';
}

function drawEvolution(p) {
  const ev = p.evolution;
  const key = JSON.stringify(ev || null);
  if (key === evoKey) return;
  evoKey = key;
  const svg = $("evo");
  svg.replaceChildren();
  evoNodes = [];
  if (!ev || !ev.path.length) {
    $("evo-meta").textContent = "ROADMAP.md has no \"Path:\" line yet.";
    return;
  }
  const byId = Object.fromEntries(ev.milestones.map((m) => [m.id, m]));
  const orbit = (m) => Math.max(48, 3.3 * m.items.length + 12);
  const hubs = new Map();
  let x = 0;
  ev.path.forEach((id, i) => {
    const r = orbit(byId[id]);
    if (i > 0) x += hubs.get(ev.path[i - 1]).r + r + 64;
    hubs.set(id, { m: byId[id], x, y: 0, r, onPath: true, i });
  });
  const maxR = Math.max(60, ...[...hubs.values()].map((h) => h.r));
  ev.side.forEach((id, k) => {
    const sx = (x * (k + 0.5)) / ev.side.length;
    hubs.set(id, { m: byId[id], x: sx, y: maxR * 2 + 170, r: orbit(byId[id]), onPath: false });
  });
  // Items round the hub from the bottom left, over the top, to the bottom
  // right, in the roadmap's order: done ones (listed first) on the left, what
  // is to come on the right, and a gap below for the hub's name.
  const GAP = 1.5;
  for (const h of hubs.values()) {
    const n = h.m.items.length;
    h.sats = h.m.items.map((it, k) => {
      const a = n === 1 ? -Math.PI / 2 : Math.PI / 2 + GAP / 2 + ((2 * Math.PI - GAP) * k) / (n - 1);
      return { it, k, a, x: h.x + h.r * Math.cos(a), y: h.y + h.r * Math.sin(a) };
    });
  }
  // the steps that ticked each item (newest first), and those whose item is gone
  const ticked = new Map();
  for (const s of ev.steps) {
    for (const d of s.done) {
      const k = `${d.milestone}/${d.item ?? "?"}`;
      if (!ticked.has(k)) ticked.set(k, []);
      ticked.get(k).push(s);
    }
  }
  const nextKey = ev.next ? `${ev.next.milestone}/${ev.next.item}` : "";
  const gBack = svgEl("g", {}, svg), gMid = svgEl("g", {}, svg), gTop = svgEl("g", {}, svg);
  let minX = Infinity, maxX = -Infinity, minY = Infinity, maxY = -Infinity;
  const grow = (px, py, m = 0) => {
    minX = Math.min(minX, px - m); maxX = Math.max(maxX, px + m);
    minY = Math.min(minY, py - m); maxY = Math.max(maxY, py + m);
  };
  // the path itself, then the side tracks' links to it
  const pathHubs = ev.path.map((id) => hubs.get(id));
  pathHubs.forEach((h, i) => {
    if (i === 0) return;
    const prev = pathHubs[i - 1], left = prev.m.total && prev.m.done === prev.m.total;
    svgEl("line", { x1: prev.x, y1: 0, x2: h.x, y2: 0,
      class: `e-spine ${left ? "travelled" : "ahead"}` }, gBack);
  });
  for (const id of ev.side) {
    const h = hubs.get(id);
    svgEl("line", { x1: h.x, y1: h.y - 30, x2: h.x, y2: maxR + 70, class: "e-side" }, gBack);
  }
  for (const h of hubs.values()) {
    const m = h.m, complete = m.total && m.done === m.total, current = m.id === ev.current;
    const behind = (ev.behind || []).includes(m.id);
    svgEl("circle", { cx: h.x, cy: h.y, r: h.r, class: "e-orbit" }, gBack);
    for (const s of h.sats) svgEl("line", { x1: h.x, y1: h.y, x2: s.x, y2: s.y, class: "e-link" }, gBack);
    // merges, just outside the items they ticked
    h.sats.forEach((s) => {
      (ticked.get(`${m.id}/${s.k}`) || []).forEach((st, j) => {
        const rr = h.r + 20 + 13 * j, px = h.x + rr * Math.cos(s.a), py = h.y + rr * Math.sin(s.a);
        svgEl("line", { x1: s.x, y1: s.y, x2: px, y2: py, class: "e-steplink" }, gMid);
        const d = svgEl("rect", { x: px - 4, y: py - 4, width: 8, height: 8,
          transform: `rotate(45 ${px} ${py})`, class: "e-step e-hit" }, gTop);
        svgEl("title", {}, d).textContent = `${st.pr ? "#" + st.pr + " " : ""}${st.title} · ${day(st.date)}`;
        evoNodes.push({ el: d, info: () => `<h3>${st.pr ? `#${st.pr} ` : ""}${esc(st.title)}</h3>
          <p class="small muted">Merged ${esc(day(st.date))}; ticked ${st.done.length} item${st.done.length === 1 ? "" : "s"}.</p>
          <p><a href="${esc(st.url)}" target="_blank" rel="noopener">Open on GitHub</a></p>` });
        grow(px, py, 6);
      });
    });
    const lost = ticked.get(`${m.id}/?`) || [];
    lost.forEach((st, j) => {
      const py = h.y - h.r - 20 - 13 * j;
      const d = svgEl("rect", { x: h.x - 4, y: py - 4, width: 8, height: 8,
        transform: `rotate(45 ${h.x} ${py})`, class: "e-step e-hit" }, gTop);
      svgEl("title", {}, d).textContent = `${st.pr ? "#" + st.pr + " " : ""}${st.title} (an item since reworded)`;
    });
    // the items
    h.sats.forEach((s) => {
      const k = `${m.id}/${s.k}`, isNext = k === nextKey;
      const c = svgEl("circle", { cx: s.x, cy: s.y, r: isNext ? 8 : 6,
        class: `e-sat e-hit ${isNext ? "next" : s.it.done ? "done" : "open"}` }, gTop);
      svgEl("title", {}, c).textContent = s.it.text;
      evoNodes.push({ el: c, info: () => `<p class="small muted">${esc(m.id)} · ${esc(shortTitle(m.title))}</p>
        <h3>${esc(s.it.text)}</h3>
        <p class="small"><b>${isNext ? "The next step" : s.it.done ? "Done" : "To do"}</b></p>
        ${s.it.done ? stepLinks(ticked.get(k) || []) : ""}` });
      grow(s.x, s.y, 10);
    });
    // the hub: progress round it, its number inside, its name below
    const pct = m.total ? (100 * m.done) / m.total : 0;
    svgEl("circle", { cx: h.x, cy: h.y, r: 31, class: "e-ringbg" }, gTop);
    svgEl("circle", { cx: h.x, cy: h.y, r: 31, class: "e-ring", pathLength: 100,
      "stroke-dasharray": `${pct} 100`, transform: `rotate(-90 ${h.x} ${h.y})` }, gTop);
    const hub = svgEl("circle", { cx: h.x, cy: h.y, r: 25,
      class: `e-hub e-hit${complete ? " complete" : ""}${current ? " current" : ""}${behind ? " behind" : ""}${h.onPath ? "" : " side"}` }, gTop);
    svgEl("title", {}, hub).textContent = `${m.id} ${m.title}: ${m.done} of ${m.total} done`;
    svgText(gTop, h.x, h.y, "e-id", m.id);
    const lines = twoLines(shortTitle(m.title), 22);
    lines.forEach((l, j) => svgText(gTop, h.x, h.y + h.r + 28 + 17 * j, "e-label", l));
    svgText(gTop, h.x, h.y + h.r + 28 + 17 * lines.length, behind ? "e-sub e-behind" : "e-sub",
      behind ? `${m.total - m.done} left behind`
        : `${m.done} of ${m.total} done${h.onPath ? "" : " · alongside"}`);
    // above everything round the hub: its items and their merges
    const stack = Math.max(lost.length, ...h.sats.map((s) => (ticked.get(`${m.id}/${s.k}`) || []).length), 0);
    const above = h.y - h.r - (stack ? 20 + 13 * (stack - 1) : 0) - 18;
    if (current) svgText(gTop, h.x, above, "e-here", "▼ you are here");
    const open = m.items.filter((i) => !i.done);
    evoNodes.push({ el: hub, info: () => `<p class="small muted">${h.onPath ? `Step ${h.i + 1} of ${ev.path.length} on the path${current ? ", in progress" : behind ? ", passed with items left" : ""}` : "Alongside the path"}</p>
      <h3>${esc(m.id)} ${esc(m.title.replace(/:\s*done$/i, ""))}</h3>
      <p class="small">${m.done} of ${m.total} done.</p>
      ${open.length ? `<p class="small"><b>Still to do</b></p><ul class="plain small">${open.map((i) => `<li>${esc(i.text)}</li>`).join("")}</ul>` : ""}` });
    grow(h.x, h.y, h.r + 30);
    grow(h.x, h.y + h.r + 28 + 17 * (lines.length + 1), 80);
    grow(h.x, above - 16, 0);
  }
  evoBox = { x: minX, y: minY, w: maxX - minX, h: maxY - minY };
  if (!evoView) fitView(); else setView(evoView);
  // the line above: where the work stands
  const cur = byId[ev.current];
  let nextItem = ev.next ? byId[ev.next.milestone].items[ev.next.item].text.replace(/\s*\(.*$/, "") : "";
  if (nextItem.length > 90) nextItem = nextItem.slice(0, 89) + "…";
  const left = (ev.behind || []).reduce((n, id) => n + byId[id].total - byId[id].done, 0);
  $("evo-meta").textContent = (cur
    ? `Now: ${cur.id} ${shortTitle(cur.title)}${nextItem ? ` · next: ${nextItem}` : ""}`
    : "Every milestone on the path is done.")
    + (left ? ` · ${left} item${left === 1 ? "" : "s"} left behind` : "");
  // the drift strip
  const d = ev.drift.slice(-40);
  const name = { path: "moved the path forward", side: "advanced a side track", planned: "planned the path",
                 other: "ticked nothing on the roadmap" };
  $("drift").innerHTML = d.map((r) => `<a class="d-${r.kind}" href="${esc(r.url)}" target="_blank" rel="noopener"
      title="#${r.number} ${esc(r.title)} · ${esc(day(r.merged))}: ${name[r.kind]}"></a>`).join("");
  const count = (k) => d.filter((r) => r.kind === k).length;
  const parts = [`${count("path")} of the last ${d.length} merges moved the path forward`];
  if (count("side")) parts.push(`${count("side")} a side track`);
  if (count("planned")) parts.push(`${count("planned")} planned it`);
  parts.push(`${count("other")} ticked nothing (fixes, upkeep or detours)`);
  $("drift-text").textContent = d.length ? parts.join(", ") + "." : "";
}

// pan, zoom, click
(function evoControls() {
  const svg = $("evo");
  if (!svg) return;
  let drag = null;
  const world = (ev) => {
    const r = svg.getBoundingClientRect();
    return { x: evoView.x + ((ev.clientX - r.left) / r.width) * evoView.w,
             y: evoView.y + ((ev.clientY - r.top) / r.height) * evoView.h, r };
  };
  svg.addEventListener("wheel", (ev) => {
    if (!evoView) return;
    ev.preventDefault();
    const f = ev.deltaY > 0 ? 1.15 : 1 / 1.15, w = world(ev);
    setView({ x: w.x - (w.x - evoView.x) * f, y: w.y - (w.y - evoView.y) * f,
              w: evoView.w * f, h: evoView.h * f });
  }, { passive: false });
  svg.addEventListener("pointerdown", (ev) => {
    if (!evoView) return;
    drag = { x: ev.clientX, y: ev.clientY, view: { ...evoView }, moved: false };
    svg.setPointerCapture(ev.pointerId);
  });
  svg.addEventListener("pointermove", (ev) => {
    if (!drag) return;
    const r = svg.getBoundingClientRect(), dx = ev.clientX - drag.x, dy = ev.clientY - drag.y;
    if (Math.abs(dx) + Math.abs(dy) > 4) drag.moved = true;
    if (drag.moved) {
      svg.classList.add("dragging");
      setView({ ...drag.view, x: drag.view.x - (dx / r.width) * drag.view.w,
                y: drag.view.y - (dy / r.height) * drag.view.h });
    }
  });
  svg.addEventListener("pointerup", (ev) => {
    svg.classList.remove("dragging");
    const wasDrag = drag && drag.moved;
    drag = null;
    if (wasDrag) return;
    const hit = document.elementFromPoint(ev.clientX, ev.clientY);
    const node = evoNodes.find((n) => n.el === hit);
    svg.querySelectorAll(".e-sel").forEach((e) => e.classList.remove("e-sel"));
    if (node) {
      node.el.classList.add("e-sel");
      evoInfo(node.info());
    } else evoInfo("");
  });
  $("evo-fit").addEventListener("click", fitView);
  $("evo-info").addEventListener("click", (ev) => {
    if (ev.target.classList.contains("evo-close")) evoInfo("");
  });
  window.addEventListener("resize", () => { if (evoView) fitView(); });
})();

// -- the loop -------------------------------------------------------------------

// <details> remember being opened or closed, across redraws
document.addEventListener("toggle", (ev) => {
  const d = ev.target;
  if (d.tagName === "DETAILS" && d.dataset.key) opened.set(d.dataset.key, d.open);
}, true);

function restoreOpen() {
  document.querySelectorAll("details[data-key]").forEach((d) => {
    if (opened.has(d.dataset.key)) d.open = opened.get(d.dataset.key);
  });
}

// Redraws replace the HTML: keep what was typed, which reply boxes were open,
// and where the cursor was.
function keepTyping() {
  document.querySelectorAll("textarea[data-key]").forEach((t) => drafts.set(t.dataset.key, t.value));
  const a = document.activeElement;
  return a && a.tagName === "TEXTAREA" && a.dataset.key ? a.dataset.key : null;
}

function restoreTyping(focused) {
  replies.forEach((id) => { const r = $(`reply-${id}`); if (r) r.classList.add("open"); });
  document.querySelectorAll("textarea[data-key]").forEach((t) => {
    if (drafts.has(t.dataset.key)) {
      t.value = drafts.get(t.dataset.key);
      if (t.value) t.closest(".reply").classList.add("open");
    }
    if (t.dataset.key === focused) t.focus();
  });
}

async function refresh() {
  try {
    page = await api("/api/overview");
    const focused = keepTyping();
    drawTop(page);
    drawInbox(page);
    drawBoard(page);
    drawRuns(page);
    drawSide(page);
    drawEvolution(page);
    restoreOpen();
    restoreTyping(focused);
  } catch (e) {
    $("status").className = "pill down";
    $("status-text").textContent = `Dashboard error: ${e.message}`;
  }
}

refresh().then(followLive);
setInterval(refresh, 10000);
setInterval(followLive, 2500);

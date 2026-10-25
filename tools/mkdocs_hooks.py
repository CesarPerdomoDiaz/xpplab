"""MkDocs hooks: make the in-app manual pages read well on the web.

The desktop app's manual uses two special link targets:
    [Open the model](example:lecar.ode)   opens an example in the app
    [Run it](cmd:run)                      runs a command in the app
On the website the first becomes a link to the model file on GitHub, and
the second becomes the command shown as code.
"""

import re

REPO = "https://github.com/CesarPerdomoDiaz/xpplab/blob/main/engine/xppaut/ode/"

_example = re.compile(r"\[([^\]]+)\]\(example:([^)]+)\)")
_command = re.compile(r"\[([^\]]+)\]\(cmd:([^)]+)\)")


def on_page_markdown(markdown, page, config, files):
    markdown = _example.sub(lambda m: f"[{m.group(1)}]({REPO}{m.group(2)})",
                            markdown)
    markdown = _command.sub(lambda m: f"{m.group(1)} (`{m.group(2)}`)",
                            markdown)
    return markdown

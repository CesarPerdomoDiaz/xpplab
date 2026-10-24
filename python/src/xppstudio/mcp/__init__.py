"""XPP Studio as a Model Context Protocol (MCP) server, for coding agents.

Run it over stdio, the way Claude Code, Codex and Claude Desktop start
MCP servers::

    python -m xppstudio.mcp [--workdir DIR]

It needs the optional dependencies: ``pip install "xppstudio[mcp]"``. The
tools themselves live in :mod:`xppstudio.mcp.session` and work without the
MCP SDK; :mod:`xppstudio.mcp.server` exposes them over MCP. See
docs/agents.md.
"""

from __future__ import annotations

from .session import Session, ToolError

__all__ = ["Session", "ToolError", "create_server", "main"]


def create_server(session: Session | None = None):
    """The MCP server object (needs the ``mcp`` package)."""
    from .server import create_server as _create
    return _create(session)


def main(argv: list[str] | None = None) -> int:
    """Command-line entry point: serve over stdio."""
    try:
        from .server import main as _main
    except ImportError as err:
        if getattr(err, "name", "") and err.name.split(".")[0] in ("mcp", "pydantic"):
            import sys
            print("The XPP Studio MCP server needs the MCP SDK: "
                  'pip install "xppstudio[mcp]" (Python 3.10 or later).',
                  file=sys.stderr)
            return 1
        raise
    return _main(argv)

"""Small helpers for the ``_repr_html_`` methods (Jupyter rich display).

Every piece of text goes through :func:`esc`, so model and variable names
cannot inject markup. Nothing here touches the engine.
"""

from __future__ import annotations

from html import escape

#: Longest table shown; the rest is summarised in a line below it.
MAX_ROWS = 20


def esc(x) -> str:
    return escape(str(x), quote=True)


def table(header, rows, total: int | None = None) -> str:
    """A ``<table>``. ``header`` and ``rows`` hold plain values, escaped here.
    If ``total`` exceeds the rows given, a line says how many are left out."""
    out = ["<table>"]
    if header:
        out.append("<thead><tr>"
                   + "".join(f"<th>{esc(h)}</th>" for h in header)
                   + "</tr></thead>")
    out.append("<tbody>")
    for r in rows:
        out.append("<tr>" + "".join(f"<td>{esc(c)}</td>" for c in r) + "</tr>")
    out.append("</tbody></table>")
    if total is not None and total > len(rows):
        out.append(f"<p>... and {total - len(rows)} more</p>")
    return "".join(out)


def block(title: str, *parts: str) -> str:
    """A ``<div>`` with a bold title line followed by ready-made HTML parts."""
    return f"<div><p><b>{esc(title)}</b></p>" + "".join(parts) + "</div>"


def fmt(x) -> str:
    """A number as plain text (``table`` escapes it)."""
    try:
        return f"{float(x):.6g}"
    except (TypeError, ValueError):
        return str(x)

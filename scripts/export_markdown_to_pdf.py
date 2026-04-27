from __future__ import annotations

import re
import sys
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4, landscape
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import LongTable, Paragraph, Preformatted, SimpleDocTemplate, Spacer, TableStyle


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUTS = [
    ROOT / "docs" / "Pakiety_Wdrozeniowe_Wariant_B.md",
    ROOT / "docs" / "Porownanie_Stary_vs_Nowy_Soft.md",
]


def build_styles():
    styles = getSampleStyleSheet()
    styles.add(
        ParagraphStyle(
            name="DocTitle",
            parent=styles["Title"],
            fontName="Helvetica-Bold",
            fontSize=20,
            leading=24,
            spaceAfter=10,
            textColor=colors.HexColor("#102542"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="H1Wide",
            parent=styles["Heading1"],
            fontName="Helvetica-Bold",
            fontSize=15,
            leading=18,
            spaceBefore=10,
            spaceAfter=8,
            textColor=colors.HexColor("#12355B"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="H2Wide",
            parent=styles["Heading2"],
            fontName="Helvetica-Bold",
            fontSize=12,
            leading=15,
            spaceBefore=8,
            spaceAfter=6,
            textColor=colors.HexColor("#134074"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="BodyWide",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=9,
            leading=12,
            spaceAfter=5,
            textColor=colors.HexColor("#222222"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="ListWide",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=9,
            leading=12,
            leftIndent=14,
            firstLineIndent=-10,
            spaceAfter=3,
            textColor=colors.HexColor("#222222"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="TableHeader",
            parent=styles["BodyText"],
            fontName="Helvetica-Bold",
            fontSize=8,
            leading=10,
            textColor=colors.white,
        )
    )
    styles.add(
        ParagraphStyle(
            name="TableCell",
            parent=styles["BodyText"],
            fontName="Helvetica",
            fontSize=8,
            leading=10,
            textColor=colors.HexColor("#1F1F1F"),
        )
    )
    styles.add(
        ParagraphStyle(
            name="CodeBlock",
            fontName="Courier",
            fontSize=8,
            leading=10,
            leftIndent=6,
            rightIndent=6,
            spaceBefore=2,
            spaceAfter=6,
            textColor=colors.HexColor("#1B1B1B"),
        )
    )
    return styles


def format_inline(text: str) -> str:
    parts = re.split(r"(`[^`]+`)", text)
    chunks = []
    for part in parts:
        if not part:
            continue
        if part.startswith("`") and part.endswith("`"):
            code = escape(part[1:-1])
            chunks.append(f'<font name="Courier">{code}</font>')
        else:
            escaped = escape(part)
            escaped = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", escaped)
            chunks.append(escaped)
    return "".join(chunks)


def is_table_delimiter(line: str) -> bool:
    stripped = line.strip()
    return bool(stripped) and all(char in "|:- " for char in stripped)


def is_table_start(lines: list[str], index: int) -> bool:
    if index + 1 >= len(lines):
        return False
    current = lines[index].strip()
    next_line = lines[index + 1].strip()
    return current.startswith("|") and next_line.startswith("|") and is_table_delimiter(next_line)


def is_special_start(lines: list[str], index: int) -> bool:
    if index >= len(lines):
        return False
    line = lines[index]
    stripped = line.strip()
    if not stripped:
        return True
    if stripped.startswith("```"):
        return True
    if re.match(r"^#{1,6}\s+", stripped):
        return True
    if re.match(r"^\d+\.\s+", stripped):
        return True
    if stripped.startswith("- "):
        return True
    return is_table_start(lines, index)


def split_table_row(line: str) -> list[str]:
    trimmed = line.strip().strip("|")
    return [cell.strip() for cell in trimmed.split("|")]


def add_table(story, rows: list[list[str]], styles, usable_width: float):
    if not rows:
        return
    col_count = max(len(row) for row in rows)
    normalized = []
    for row_index, row in enumerate(rows):
        padded = row + [""] * (col_count - len(row))
        style_name = "TableHeader" if row_index == 0 else "TableCell"
        normalized.append([Paragraph(format_inline(cell), styles[style_name]) for cell in padded])

    col_width = usable_width / col_count
    table = LongTable(
        normalized,
        colWidths=[col_width] * col_count,
        repeatRows=1,
        splitByRow=1,
        hAlign="LEFT",
    )
    table.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#1F4E79")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#F5F8FC")]),
                ("GRID", (0, 0), (-1, -1), 0.35, colors.HexColor("#A9B8C8")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 5),
                ("RIGHTPADDING", (0, 0), (-1, -1), 5),
                ("TOPPADDING", (0, 0), (-1, -1), 4),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 4),
            ]
        )
    )
    story.append(table)
    story.append(Spacer(1, 5))


def markdown_to_story(text: str, styles, usable_width: float):
    lines = text.splitlines()
    story = []
    i = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        if not stripped:
            i += 1
            continue

        if stripped.startswith("```"):
            i += 1
            code_lines = []
            while i < len(lines) and not lines[i].strip().startswith("```"):
                code_lines.append(lines[i])
                i += 1
            if i < len(lines):
                i += 1
            story.append(Preformatted("\n".join(code_lines), styles["CodeBlock"]))
            story.append(Spacer(1, 4))
            continue

        heading = re.match(r"^(#{1,6})\s+(.*)$", stripped)
        if heading:
            level = len(heading.group(1))
            content = heading.group(2).strip()
            if level == 1:
                style = styles["DocTitle"]
            elif level == 2:
                style = styles["H1Wide"]
            else:
                style = styles["H2Wide"]
            story.append(Paragraph(format_inline(content), style))
            i += 1
            continue

        if is_table_start(lines, i):
            table_lines = []
            while i < len(lines) and lines[i].strip().startswith("|"):
                table_lines.append(lines[i].strip())
                i += 1
            rows = []
            for row_index, table_line in enumerate(table_lines):
                if row_index == 1 and is_table_delimiter(table_line):
                    continue
                rows.append(split_table_row(table_line))
            add_table(story, rows, styles, usable_width)
            continue

        ordered = re.match(r"^(\d+)\.\s+(.*)$", stripped)
        if ordered:
            item_no = 1
            while i < len(lines):
                candidate = lines[i].strip()
                match = re.match(r"^\d+\.\s+(.*)$", candidate)
                if not match:
                    break
                story.append(Paragraph(f"{item_no}. {format_inline(match.group(1).strip())}", styles["ListWide"]))
                item_no += 1
                i += 1
            story.append(Spacer(1, 2))
            continue

        if stripped.startswith("- "):
            while i < len(lines) and lines[i].strip().startswith("- "):
                item = lines[i].strip()[2:].strip()
                story.append(Paragraph(f"&#8226; {format_inline(item)}", styles["ListWide"]))
                i += 1
            story.append(Spacer(1, 2))
            continue

        paragraph_lines = [stripped]
        i += 1
        while i < len(lines) and not is_special_start(lines, i):
            paragraph_lines.append(lines[i].strip())
            i += 1
        paragraph_text = " ".join(part for part in paragraph_lines if part)
        story.append(Paragraph(format_inline(paragraph_text), styles["BodyWide"]))

    return story


def draw_footer(source_name: str):
    def _draw(canvas, doc):
        canvas.saveState()
        canvas.setFont("Helvetica", 8)
        canvas.setFillColor(colors.HexColor("#5B6572"))
        canvas.drawString(doc.leftMargin, 8 * mm, source_name)
        canvas.drawRightString(doc.pagesize[0] - doc.rightMargin, 8 * mm, f"Strona {canvas.getPageNumber()}")
        canvas.restoreState()

    return _draw


def export_markdown(input_path: Path):
    output_path = input_path.with_suffix(".pdf")
    styles = build_styles()
    doc = SimpleDocTemplate(
        str(output_path),
        pagesize=landscape(A4),
        leftMargin=12 * mm,
        rightMargin=12 * mm,
        topMargin=12 * mm,
        bottomMargin=14 * mm,
        title=input_path.stem,
        author="Codex",
    )
    story = markdown_to_story(input_path.read_text(encoding="utf-8"), styles, doc.width)
    doc.build(story, onFirstPage=draw_footer(input_path.name), onLaterPages=draw_footer(input_path.name))
    return output_path


def main():
    inputs = [Path(arg).resolve() for arg in sys.argv[1:]] if len(sys.argv) > 1 else DEFAULT_INPUTS
    created = []
    for input_path in inputs:
        if not input_path.exists():
            raise FileNotFoundError(f"Missing input file: {input_path}")
        created.append(export_markdown(input_path))

    for path in created:
        print(path)


if __name__ == "__main__":
    main()

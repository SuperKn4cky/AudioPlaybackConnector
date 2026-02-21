#!/usr/bin/env python
import ast
import sys

FNV1_32_INIT = 0x811c9dc5
FNV_32_PRIME = 0x01000193


def fnv1a_32(data, hval=FNV1_32_INIT):
    for byte in data:
        hval ^= byte
        hval = (hval * FNV_32_PRIME) & 0xffffffff
    return hval

def parse_po_quoted(raw):
    return ast.literal_eval(raw.strip())


def parse_po(path):
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    entries = []
    state = None
    current = {"msgctxt": None, "msgid": None, "msgstr": None, "fuzzy": False}

    def flush_entry():
        if current["msgid"] is None:
            return
        entries.append(
            {
                "msgctxt": current["msgctxt"],
                "msgid": current["msgid"],
                "msgstr": current["msgstr"] or "",
                "fuzzy": current["fuzzy"],
            }
        )
        current["msgctxt"] = None
        current["msgid"] = None
        current["msgstr"] = None
        current["fuzzy"] = False

    for line in lines:
        stripped = line.strip()
        if not stripped:
            flush_entry()
            state = None
            continue
        if stripped.startswith("#,"):
            if "fuzzy" in stripped:
                current["fuzzy"] = True
            continue
        if stripped.startswith("#"):
            continue
        if stripped.startswith("msgctxt "):
            current["msgctxt"] = parse_po_quoted(stripped[len("msgctxt ") :])
            state = "msgctxt"
            continue
        if stripped.startswith("msgid "):
            if current["msgid"] is not None and current["msgstr"] is not None:
                flush_entry()
            current["msgid"] = parse_po_quoted(stripped[len("msgid ") :])
            state = "msgid"
            continue
        if stripped.startswith("msgstr["):
            index_end = stripped.find("]")
            if index_end != -1:
                index = stripped[len("msgstr[") : index_end]
                value_part = stripped[index_end + 1 :].strip()
                if value_part.startswith('"') and index == "0":
                    current["msgstr"] = parse_po_quoted(value_part)
                    state = "msgstr"
                else:
                    state = None
            continue
        if stripped.startswith("msgstr "):
            current["msgstr"] = parse_po_quoted(stripped[len("msgstr ") :])
            state = "msgstr"
            continue
        if stripped.startswith('"'):
            value = parse_po_quoted(stripped)
            if state == "msgctxt":
                current["msgctxt"] = (current["msgctxt"] or "") + value
            elif state == "msgid":
                current["msgid"] = (current["msgid"] or "") + value
            elif state == "msgstr":
                current["msgstr"] = (current["msgstr"] or "") + value

    flush_entry()
    return entries


def po2ymo(infile, outfile, includefuzzy=False, encoding="utf-16le"):
    entries = parse_po(infile)

    units = {}
    for entry in entries:
        if not entry["msgid"] or not entry["msgstr"]:
            continue
        if entry["fuzzy"] and not includefuzzy:
            continue

        source = entry["msgid"]
        if entry["msgctxt"]:
            source = entry["msgctxt"] + "\004" + source
        hash_value = fnv1a_32(source.encode(encoding))
        units[hash_value] = entry["msgstr"].encode(encoding) + bytes(2)

    if len(units) > 0xFFFF:
        raise ValueError("too many translation entries")

    byteorder = "little"
    outfile.write(len(units).to_bytes(2, byteorder))

    offset = 2 + len(units) * (4 + 2)
    for hash_value, data in units.items():
        if offset > 0xFFFF:
            raise ValueError("translation data too large for YMO format")
        outfile.write(hash_value.to_bytes(4, byteorder))
        outfile.write(offset.to_bytes(2, byteorder))
        offset += len(data)

    for data in units.values():
        outfile.write(data)

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("uasge: po2ymo.py <infile> <outfile>")
        sys.exit()
    with open(sys.argv[2], "wb") as outfile:
        po2ymo(sys.argv[1], outfile)

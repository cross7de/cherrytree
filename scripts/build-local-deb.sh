#!/usr/bin/env bash
# Build a Debian package of the local CherryTree fork without changing its
# upstream version number or globally installing files.
set -euo pipefail

usage() {
    cat <<'EOF'
Verwendung:
  ./scripts/build-local-deb.sh [DEBIAN-REVISION]

Erstellt ein Debian-Paket des lokalen CherryTree-Forks.
Standardrevision: 2+ctproj1

Beispiel:
  ./scripts/build-local-deb.sh 2+ctproj1

Die Programmversion bleibt bei CherryTree (z. B. 1.7.2).
Nur die Debian-Paketrevision wird angepasst.
Dieses Skript installiert nichts.
EOF
}

require_tools() {
    for name in cpack dpkg-deb dpkg-query dpkg; do
        command -v "$name" >/dev/null || { echo "Fehlendes Programm: $name" >&2; exit 1; }
    done
}

build_package() {
    cd "$repo"
    ./build.sh rel notest
    ( cd build && cpack -G DEB -D "CPACK_DEBIAN_PACKAGE_RELEASE=${revision}" )
}

rename_and_report() {
    local version arch source target current
    source="$(find "$repo/build" -maxdepth 1 -type f -name 'cherrytree-*-Linux.deb' | sort | tail -n 1)"
    [[ -n "$source" ]] || { echo "Kein CPack-Paket unter build/ gefunden" >&2; exit 1; }
    version="$(dpkg-deb -f "$source" Version)"
    arch="$(dpkg-deb -f "$source" Architecture)"
    target="$repo/build/cherrytree_${version}_${arch}.deb"
    mv -f -- "$source" "$target"
    if [[ -f "${source}.sha256" ]]; then rm -f -- "${source}.sha256"; fi
    ( cd "$repo/build" && sha256sum "$(basename "$target")" > "$(basename "$target").sha256" )
    printf 'Paket erstellt: %s\nPaketversion: %s\n' "$target" "$version"
    current="$(dpkg-query -W -f='${Version}' cherrytree 2>/dev/null || true)"
    if [[ -n "$current" ]]; then
        printf 'Installierte Version: %s\n' "$current"
        if dpkg --compare-versions "$version" lt "$current"; then
            echo 'ACHTUNG: Das neue Paket wäre ein Downgrade.' >&2
        fi
    fi
    printf '\nNächster Schritt (zuerst simulieren):\n  sudo apt -s install "%s"\n' "$target"
}

main() {
    if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then usage; return; fi
    [[ $# -le 1 ]] || { usage >&2; exit 2; }
    revision="${1:-2+ctproj1}"
    [[ "$revision" =~ ^[0-9]+\+[A-Za-z0-9.+~]+$ ]] || {
        echo 'Ungültige Debian-Revision. Beispiel: 2+ctproj1' >&2; exit 2;
    }
    repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
    [[ -f "$repo/build.sh" && -f "$repo/CMakeLists.txt" ]] || {
        echo 'Dieses Skript muss im CherryTree-Quellrepository liegen (scripts/).' >&2; exit 2;
    }
    require_tools
    build_package
    rename_and_report
}

main "$@"

# Shared Solution Finder Enhanced Data

This folder contains portable data used by Solution Finder Enhanced and intended
for future Windows/Linux builds.

## openers.json

`openers.json` is the opener sample database. Each entry is a single selectable
variation with:

- `id`: stable unique id
- `name`: display name
- `openerName`: group name shown in the opener selector
- `variationName`: variation shown inside the selected group
- `code`: fumen code, when available
- `cells`: optional 24x10 board cells for entries without fumen code

The macOS app loads this file first and falls back to its embedded database if
the file is missing or invalid.

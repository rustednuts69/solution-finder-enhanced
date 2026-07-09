# Native macOS Launcher

This folder contains a compiled macOS command-line launcher for solution-finder.

It is not a full manual rewrite of the 552-file Java solver into C. The C code
in `sfinder_launcher.c` is a native macOS launcher that runs the source-built
Java classes with the bundled Apache Commons CLI dependency. A true no-Java
native executable would require GraalVM `native-image` or a full C port of the
solver.

## Files

- `bin/sfinder` - arm64 Mach-O launcher compiled with clang
- `lib/sfinder-from-source.jar` - classes compiled from `solution-finder/src/main/java`
- `lib/sfinder-deps.jar` - Apache Commons CLI classes
- `sfinder_launcher.c` - C launcher source

## Run

From the workspace root:

```sh
native-macos/bin/sfinder percent -fp solution-finder-1.43/input/field.txt -pp solution-finder-1.43/input/patterns.txt
```

Java must be installed on the Mac because this launcher starts the JVM.

## Rebuild

From the workspace root:

```sh
./native-macos/rebuild.sh
```

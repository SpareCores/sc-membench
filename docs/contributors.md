# Contributors

## Building the Tool

The tool is one C file, [`membench.c`](../membench.c), built with the
[`Makefile`](../Makefile). See [Building from Source](./build.md) for the
dependencies per platform, the install commands, and every build target.

## Tests

CI runs on every push and pull request
([`ci.yml`](../.github/workflows/ci.yml)). It builds `make all` and runs the
following smoke test on Linux x86_64 and arm64, macOS arm64 and x86_64, and
FreeBSD x86_64:

```bash
./membench -R -s 1024 -r 1
```

For a quick local check, `make test` builds `membench` and runs
`./membench -v -t 30`.

## Building the Image

Build the image from the repository root:

```bash
docker build -t membench .
```

The [`Dockerfile`](../Dockerfile) works as follows:

- The build stage, on `ubuntu:24.04`, installs `build-essential`,
  `libhwloc-dev`, and `libnuma-dev`, and runs `make full`. It has no
  `libhugetlbfs` headers, so the binary is built without `libhugetlbfs`.
- The final stage, also on `ubuntu:24.04`, installs `libhwloc15`, `libnuma1`,
  `libhugetlbfs-dev`, and `libgomp1`, and copies in the binary as
  `/usr/local/bin/membench`.
- The `resource-tracker` binary is copied from the image named by the
  `RESOURCE_TRACKER_IMAGE` build argument (default:
  `ghcr.io/sparecores/resource-tracker:main`).
- The build context excludes `.git`, `.github`, any Markdown files in the
  repository root, the [`membench.1`](../membench.1) man page, and built
  binaries ([`.dockerignore`](../.dockerignore)). The `docs/` pages and
  `DESCRIPTION` stay in the context, and `COPY . .` copies them into the
  build stage.

The image workflow ([`docker.yml`](../.github/workflows/docker.yml)) builds
`linux/amd64` and `linux/arm64` images on native runners, with zstd-compressed
layers, and merges them into one multi-arch manifest. A push to `main`
publishes `ghcr.io/sparecores/membench:main` and `:latest`; a published
GitHub release also publishes an image tag named after the release version,
without its `v` prefix.

## Documentation

The `DESCRIPTION` file holds the one-sentence summary of the image. Its text
is repeated in the Images table of the `README.md` in the
[`sc-images` repository](https://github.com/SpareCores/sc-images), so change
both together. The man page, `membench.1`, is maintained separately from
these docs.

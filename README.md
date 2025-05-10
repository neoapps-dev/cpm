# CPM - C Package Manager

![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)

CPM is a lightweight, efficient package manager for C projects that handles dependency management with zero bloat.

## Features

- **Simple project initialization** - Get started in seconds
- **Effortless dependency management** - Install, update, and remove packages with ease
- **Automatic Makefile generation** - Build systems created for you
- **Minimalist design** - No unnecessary abstraction layers

## Installation

```bash
git clone https://github.com/neoapps-dev/cpm.git
cd cpm
make
sudo make install
```

## Quick Start

```bash
cpm init my-awesome-project
cpm add libfoo 1.0
cpm add slog 1.8.48
cpm install
make
```

## Usage

```
cpm - C Package Manager v0.1.0
Usage:
  cpm init [project-name]    Initialize a new project
  cpm install                Install all packages in cpmfile
  cpm install <package>      Install a specific package
  cpm add <package> <ver>    Add a package to cpmfile
  cpm remove <package>       Remove a package
  cpm update                 Update all packages
  cpm list                   List installed packages
  cpm makefile               Generate Makefile
  cpm help                   Show this help
```

## Project Structure

After initializing a project with CPM, your directory structure will look like this:

```
my-project/
├── .cpm/            # Package storage directory
├── main.c           # Main source file
├── cpmfile          # Package manifest
├── Makefile         # Auto-generated build file
└── .gitignore       # Auto-configured gitignore
```

## Package Repository

Packages are pulled from the official CPM repository at:
`https://github.com/neoapps-dev/cpm-repo`

## Contributing

Feel free to submit issues and pull requests. For major changes, please open an issue first to discuss what you'd like to change.

## License

MIT

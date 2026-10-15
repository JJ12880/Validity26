Validity
=====================================

https://github.com/RadiumCore/Validity

What is Validity?
----------------

Validity is a decentralised digital currency secured by Proof of Stake.

What is the Validity client?
----------------

Validity is the name of open source software which enables use of the Validity protocol.
It connects to the Validity peer-to-peer network to download and fully
validate blocks and transactions. It also includes a wallet and graphical user
interface, which can be optionally built.

Validity v26 is based on Blackcoin More, which is based on Bitcoin Core v26.

For more information, as well as an immediately usable, binary version of
the Validity software, see https://github.com/RadiumCore/Validity/releases.

License
-------

Validity is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is regularly built (see `doc/build-*.md` for instructions) and tested. [Tags](https://github.com/RadiumCore/Validity/tags) are created
regularly to indicate new official, stable release versions of Validity.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Bug reports and questions can be filed at https://github.com/RadiumCore/Validity/issues

Testing
-------

Testing and code review might be the bottleneck for development. Please help out by testing
other people's pull requests, and remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to
submit new unit tests for old code. Unit tests can be compiled and run
(assuming they weren't disabled in configure) with: `make check`. Further details on running
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written
in Python.
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The CI (Continuous Integration) systems make sure that every pull request is built for Windows, Linux, and macOS,
and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the
code. This is especially important for large or high-risk changes. It is useful
to add a test plan to the pull request description if testing the changes is
not straightforward.

Translations
------------

Changes to translations as well as new translations can be submitted to
the project on GitHub as pull requests or issues at https://github.com/RadiumCore/Validity.

Translations might be periodically merged into the git repository. See the
[translation process](doc/translation_process.md) for details on how this works.

Branches
-------

### develop
The develop branch is typically used by developers as the main branch for integrating new features and changes into the codebase.
Pull requests should always be made to this branch (except for critical fixes), and might possibly break the code.
The develop branch is considered an unstable branch, as it is constantly updated with new code, and it may contain bugs or unfinished features. It is not guaranteed to work properly on any system.

### master
The master branch gets latest updates from the stable branch.
However, it may contain experimental features and should be used with caution.

### 26.x
The release branch for Validity 26.x. It is intended to contain stable and functional code that has been thoroughly tested and reviewed.

### 28.x
The release branch for Validity 28.x. Contains functional but highly experimental code.

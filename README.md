# Badges


[![Chat on IRC][irc-badge]][irc-url]

[![License: GPL v2][gpl-badge]][gpl-url]

[![CodeFactor Grade][codefactor-badge]][codefactor-url]
[![CodeQL][codeql-badge]][codeql-url]

[![CI Android][android-badge]][android-url]
[![CI FreeBSD][freebsd-badge]][freebsd-url]
[![CI iOS][ios-badge]][ios-url]
[![CI Mac OS X][macosx-badge]][macosx-url]
[![CI OmniOS][omnios-badge]][omnios-url]
[![CI Ubuntu][ubuntu-badge]][ubuntu-url]
[![CI Windows MinGW][win-mingw-badge]][win-mingw-url]
[![CI Windows MSVC][win-msvc-badge]][win-msvc-url]

[![Coverity Scan Analysis][cov-analysis-badge]][cov-analysis-url]
[![Coverity Scan Results][cov-results-badge]][cov-results-url]

[![snapshots][snapshots-badge]][snapshots-url]
[![Latest snapshot][snapshot-release-badge]][snapshots-release-url]

## What is Exult?

----

Ultima VII an RPG from the early 1990's, still has a huge following. But, being a DOS game with a very nonstandard memory manager, it is difficult to run it on the latest computers. Exult is a project to create an Ultima VII game engine that runs on modern operating systems, capable of using the data and graphics files that come with the game.

Exult is written in C++ and runs on, at least, Linux, macOS and Windows using the SDL library to make porting to other platforms relatively easy. The current version supports all of "Ultima VII: The Black Gate" and "Ultima VII Part 2: Serpent Isle", allowing you to finish both games. This is only possible due to the work done by other fans who have decoded the various Ultima VII data files, especially Gary Thompson, Maxim Shatskih, Jakob Schonberg, and Wouter Dijkslag.

Exult aims to let those people who own Ultima VII (copyright 1993) play the game on modern hardware, in as close to (or perhaps even surpassing) its original splendor as is possible. You need to own "Ultima VII: The Black Gate" and/or "Ultima VII Part 2: Serpent Isle" and optionally the add-ons (not required to run) in order to use Exult, and we encourage you to buy a legal copy.

For more information, either consult the README file on the repository, or view its HTML version [here](https://exult.info/docs.html).

[irc-badge]:            https://img.shields.io/badge/web.libera.chat-%23exult-blue?logo=LiveChat&logoColor=white
[gpl-badge]:            https://img.shields.io/github/license/exult/exult?label=License&logo=GNU&logoColor=white
[codefactor-badge]:     https://img.shields.io/codefactor/grade/github/exult/exult?label=codefactor&logo=codefactor&logoColor=white
[codeql-badge]:         https://img.shields.io/github/actions/workflow/status/exult/exult/codeql.yml?label=CodeQL&logo=github&logoColor=white
[android-badge]:        https://img.shields.io/github/actions/workflow/status/exult/exult/ci-android.yml?label=CI%20Android&logo=Android&logoColor=white
[freebsd-badge]:        https://img.shields.io/github/actions/workflow/status/exult/exult/ci-freebsd.yml?label=CI%20FreeBSD&logo=freebsd&logoColor=white
[ios-badge]:            https://img.shields.io/github/actions/workflow/status/exult/exult/ci-ios.yml?label=CI%20iOS&logo=iOS&logoColor=white
[macosx-badge]:         https://img.shields.io/github/actions/workflow/status/exult/exult/ci-macos.yml?label=CI%20Mac%20OS%20X&logo=Apple&logoColor=white
[ubuntu-badge]:         https://img.shields.io/github/actions/workflow/status/exult/exult/ci-linux.yml?label=CI%20Ubuntu&logo=Ubuntu&logoColor=white
[win-mingw-badge]:      https://img.shields.io/github/actions/workflow/status/exult/exult/ci-windows.yml?label=CI%20Windows%2FMinGW&logo=Windows&logoColor=white
[win-msvc-badge]:       https://img.shields.io/github/actions/workflow/status/exult/exult/ci-msvc.yml?label=CI%20Windows%2FMSVC&logo=Windows&logoColor=white
[omnios-badge]:         https://img.shields.io/github/actions/workflow/status/exult/exult/ci-omnios.yml?label=CI%20OmniOS&logoColor=white&logo=data:image/svg%2bxml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSI1MTIiIGhlaWdodD0iNTEyIj48ZyBzdHlsZT0iZmlsbDojZmZmIj48cGF0aCBkPSJNMzE2LjcwOCA1MDcuOTIxYy0zNC40ODItNi4yODQtNTQuMzM1LTEzLjg0Ni03OC40NDMtMjkuODc4LTE0LjU1LTkuNjc2LTMyLjc0LTI2LjkxNy00Mi44NjYtNDAuNjMzLTIyLjI1My0zMC4xNC0zMy41MjQtNjcuODQxLTMxLjg1Ni0xMDYuNTU3IDEuOS00NC4xMDQgMTcuNzY3LTc5Ljk1MyA0OS4xODEtMTExLjEyIDI0LjktMjQuNzA0IDUzLjAyLTQwLjA0IDg4LjUzLTQ4LjI4MiAxNC44OC0zLjQ1NCA0Ny43OTctNC4zIDYzLjc5NS0xLjY0IDcwLjY2NyAxMS43NTIgMTI2LjI1OCA2MS42NSAxNDIuMzYgMTI3Ljc4IDUuOTUgMjQuNDM3IDYuMzUgNTEuMjggMS4xNDIgNzYuNTk4LTEzLjIwNiA2NC4xOTctNjUuMzU4IDExNS4yNjUtMTMzLjg4NCAxMzEuMTAxLTcuNzIzIDEuNzg1LTE1LjQ5IDIuNDQyLTMyLjU3NCAyLjc1Ni0xMi4zMzguMjI3LTIzLjc2Mi4xNzEtMjUuMzg1LS4xMjV6bTQ2LjYzNy03NS4wMTFjMTYuNTkyLTQuNTMzIDMyLjk2My0xNS4yODMgNDQuMzItMjkuMTAzIDYuOTM2LTguNDQyIDE1LjUzNC0yNS4yNyAxOC4xNTItMzUuNTMgNi42MzctMjYuMDEgNC4yNC01NC41MDEtNi40MTktNzYuMjgyLTE3LjkzLTM2LjYzOC01My4xMjgtNTUuOTktOTMuMDUzLTUxLjE2My00MS42NDggNS4wMzYtNzMuNTc5IDM3Ljg5My03OS4zOSA4MS42OTItMS43NzIgMTMuMzU1LS43MzggMzIuMjc2IDIuNDM3IDQ0LjYyIDguOTQzIDM0Ljc2NiAzNC42MjMgNTkuODg3IDY4LjQ5NyA2Ny4wMDIgMTAuMDQ3IDIuMTEgMzUuNzY1IDEuNDEgNDUuNDU2LTEuMjM2eiIgc3R5bGU9ImRpc3BsYXk6aW5saW5lO2ZpbGw6I2ZmZjtzdHJva2Utd2lkdGg6MS4xNzc0MyIgdHJhbnNmb3JtPSJtYXRyaXgoMS4wMDQgMCAwIDEuMDE1IC0yLjE0NCAtMy45MSkiLz48cGF0aCBkPSJNMTQzLjE0NyAzOTYuMzk5Yy0yNi4zNzEtOC43MDgtNTQuNTUtMjQuODIyLTc1Ljg2NC00My4zODZDMzYuMDQ4IDMyNS44MSAxNC4wOTkgMjg3Ljk4OCA0Ljc4NiAyNDUuMzJjLTIuMy0xMC41NC0yLjYzLTE1LjUzLTIuNjUtNDAuMjE0LS4wMi0yNS4wMTIuMjgtMjkuNTY3IDIuNjcyLTQwLjUxIDcuMTg1LTMyLjg3NiAxOS43MzUtNTkuMzIgMzkuOTUyLTg0LjE4NCAzMy4wODMtNDAuNjg2IDgwLjQ5Ny02Ny4xOCAxMzQuMzEzLTc1LjA0OCAxNy45NzItMi42MjggNTIuOTk3LTEuNzEgNjkuNDY3IDEuODIzIDI3Ljg3NiA1Ljk3NyA1OS41NiAxOS42NTQgODEuMDYxIDM0Ljk5MiAzMS4yNTQgMjIuMjk1IDU2Ljg2IDU0LjM2NiA3MC4yMjcgODcuOTU2IDQuMjQ2IDEwLjY2OSAxMC45NzMgMzQuMDYgMTAuMDQ3IDM0LjkzNS0uMjY2LjI1LTUuNzk2LTEuMzAyLTEyLjI5LTMuNDUtMjEuMzYyLTcuMDY5LTQ2LjYxMi0xMC43MjMtNjYuMjcyLTkuNTkxbC0xMC43NzkuNjItNC44ODQtOS44MDJjLTYuMzEyLTEyLjY2Ny0xNi4zODktMjYuMDUzLTI3LjM4Mi0zNi4zNzMtMjEuNTU1LTIwLjIzNS00NC4yOTEtMjkuNjUtNzQuNDYtMzAuODMzLTM4LjEyNS0xLjQ5Ni02Ni43NSA5LjYwNC05MS44NjggMzUuNjI0LTE1LjYwMiAxNi4xNjItMjUuNjg0IDMzLjc0NC0zMS4yODUgNTQuNTU2LTkuMzc0IDM0LjgzOC01LjgwMyA3My44MzggOS41MDYgMTAzLjgwNyA5LjAwNiAxNy42MyAyNC41NDggMzUuMjU0IDM5Ljc4MiA0NS4xMDhsNy45MjcgNS4xMjl2MTguNTMyYzAgMTkuNzEyIDIuNDUgMzguMDY3IDcuMDQ4IDUyLjgxNyAyLjY0MSA4LjQ3MiAyLjc5NSA5LjQyIDEuNTEyIDkuMzM2LS40ODctLjAzMi02LjQ2NC0xLjktMTMuMjgzLTQuMTUxem0xMzMuMTc0LTcuMjIzYy00LjY0MS02Ljk3NS04Ljc0MS0xNS41MDQtMTAuODczLTIyLjYyLTIuNjc0LTguOTIxLTQuMjkzLTI1LjkyNS0zLjQzMy0zNi4wNWwuNzE4LTguNDU1IDcuNzM1LTQuNDZjMTYuNjM2LTkuNTkzIDM3LjM2MS0zMy4wMjEgNDYuNjk3LTUyLjc4NyAzLjk2OS04LjQwMyA0LjUzMi04LjYzIDIxLjIzOC04LjYwNCAxMi43NjguMDIgMjMuMDIgMi4yMSAzMi4zOTkgNi45MTkgOC4xODUgNC4xMDkgMTguNTUgMTIuMjk3IDIzLjEzNyAxOC4yNzdsMy4zMjYgNC4zMzctMi4xODMgNC44OTJjLTE3LjU1IDM5LjM0LTU0LjQ2MiA3NS41MjgtOTcuOTI5IDk2LjAxLTE2LjkyNyA3Ljk3Ni0xNy4xOTQgOC4wMDktMjAuODMyIDIuNTQxeiIgc3R5bGU9ImRpc3BsYXk6aW5saW5lO2ZpbGw6I2ZmZjtzdHJva2Utd2lkdGg6MS4xNzc0MyIgdHJhbnNmb3JtPSJtYXRyaXgoMS4wMDQgMCAwIDEuMDE1IC0yLjE0NCAtMy45MSkiLz48L2c+PC9zdmc+
[cov-analysis-badge]:   https://img.shields.io/github/actions/workflow/status/exult/exult/coverity-scan.yml?label=Coverity%20Scan%20Analysis&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyB2aWV3Qm94PSIwIDAgMjU2IDI1MiIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBkPSJNMjYuOTUgMTA5LjA4bC0zLjUyLTkuNDUgMzcuOTYgNzAuODloLjg1bDQ3LjMzLTExOC4xM2MuODMtMi41NiA4LjI2LTIxLjc0IDguNTEtMzAuMi42My0yMS44NC0xNC4xLTIzLjgxLTI5Ljc3LTE5LjM5QzM2Ljg3IDE5LjQ2LS4yNCA2Ny44My4wMSAxMjQuNzhjLjIgNTIuOTcgMzIuNjQgOTguMjQgNzguNjUgMTE3LjM4TDI2Ljk1IDEwOS4wOE0xNzQuMzMgNS40OGMtNi4zMiAxMi43LTEzLjEgMjYuMzctMjEuNjggNDguMDhMNzkuMjIgMjQyLjM5YzE1LjA5IDYuMiAzMS42MyA5LjYgNDguOTYgOS41MiA3MC41LS4yNyAxMjcuNDItNTcuNjcgMTI3LjEzLTEyOC4xOC0uMjItNTMuODMtMzMuNzYtOTkuNy04MC45OC0xMTguMjYiIGZpbGw9IiNmZmYiLz48L3N2Zz4=
[cov-results-badge]:    https://img.shields.io/coverity/scan/10872?label=Coverity%20Scan%20Results&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyB2aWV3Qm94PSIwIDAgMjU2IDI1MiIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj48cGF0aCBkPSJNMjYuOTUgMTA5LjA4bC0zLjUyLTkuNDUgMzcuOTYgNzAuODloLjg1bDQ3LjMzLTExOC4xM2MuODMtMi41NiA4LjI2LTIxLjc0IDguNTEtMzAuMi42My0yMS44NC0xNC4xLTIzLjgxLTI5Ljc3LTE5LjM5QzM2Ljg3IDE5LjQ2LS4yNCA2Ny44My4wMSAxMjQuNzhjLjIgNTIuOTcgMzIuNjQgOTguMjQgNzguNjUgMTE3LjM4TDI2Ljk1IDEwOS4wOE0xNzQuMzMgNS40OGMtNi4zMiAxMi43LTEzLjEgMjYuMzctMjEuNjggNDguMDhMNzkuMjIgMjQyLjM5YzE1LjA5IDYuMiAzMS42MyA5LjYgNDguOTYgOS41MiA3MC41LS4yNyAxMjcuNDItNTcuNjcgMTI3LjEzLTEyOC4xOC0uMjItNTMuODMtMzMuNzYtOTkuNy04MC45OC0xMTguMjYiIGZpbGw9IiNmZmYiLz48L3N2Zz4=
[snapshots-badge]:      https://github.com/exult/exult/actions/workflows/snapshots.yml/badge.svg
[snapshot-release-badge]:  https://img.shields.io/github/release-date-pre/exult/exult?display_date=published_at&label=Latest%20snapshot%20for%20Windows%20and%20Android

[irc-url]:              https://web.libera.chat/#exult
[gpl-url]:              https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html
[codefactor-url]:       https://www.codefactor.io/repository/github/exult/exult
[codeql-url]:           https://github.com/exult/exult/actions/workflows/codeql.yml
[android-url]:          https://github.com/exult/exult/actions/workflows/ci-android.yml
[freebsd-url]:          https://github.com/exult/exult/actions/workflows/ci-freebsd.yml
[ios-url]:              https://github.com/exult/exult/actions/workflows/ci-ios.yml
[macosx-url]:           https://github.com/exult/exult/actions/workflows/ci-macos.yml
[omnios-url]:           https://github.com/exult/exult/actions/workflows/ci-omnios.yml
[ubuntu-url]:           https://github.com/exult/exult/actions/workflows/ci-linux.yml
[win-mingw-url]:        https://github.com/exult/exult/actions/workflows/ci-windows.yml
[win-msvc-url]:         https://github.com/exult/exult/actions/workflows/ci-msvc.yml
[cov-analysis-url]:     https://github.com/exult/exult/actions/workflows/coverity-scan.yml
[cov-results-url]:      https://scan.coverity.com/projects/exult-exult
[snapshots-url]:        https://github.com/exult/exult/actions/workflows/snapshots.yml
[snapshots-release-url]:    https://github.com/exult/exult/releases?q=prerelease%3Atrue

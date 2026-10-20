# Contributing to Exult

> Draft for discussion. Remove this note once the team agrees on the guidelines.

Bug reports, testing, documentation, and patches are welcome! Check the [documentation](README), [FAQ](FAQ), and existing [issues](https://github.com/exult/exult/issues) before starting. Use [Discussions](https://github.com/exult/exult/discussions) for questions and talk substantial changes through with the team early. Draft pull requests are welcome for code, documentation, and policy proposals.

## Original engine code and decompilations

Exult runs the original Ultima VII game data with its own engine. An independent engine lets us support modern systems, avoid licensing issues with the original engine code, and develop enhancements and tools for new content. Examples include Serpent Isle-style paperdolls in The Black Gate, support for multiple maps, and a modding framework used by projects such as the Keyring mod and the Ultima VI Remake.

These guidelines apply to contributors, reviewers, and maintainers alike. Please don't use original engine source code, including leaked source, or material obtained by disassembling, decompiling, or reconstructing the original executables as implementation references. Please don't post or link to that material in issues, pull requests, or discussions, including through mirrors or indirect download instructions. This includes excerpts, pseudocode, translations, and implementation explanations derived from those sources, whether written by people or AI or repeated in wikis, forums, or notes.

Changing names, rewriting from memory, removing links, or having another person or tool translate the material doesn't make a contribution independent. Public availability or a license label on a reconstruction isn't enough for us to use it.

Work on game data formats and usecode can continue through Exult's existing tools and workflows. The original engine's implementation of usecode intrinsics and other engine behavior is covered by the rules above. Explain where new material comes from and only include code, scripts, or assets we can redistribute.

## Reporting bugs and checking comparisons

Report one problem per issue. Try a recent snapshot or development build where practical, and include:

- Exult version or commit, operating system, and relevant settings.
- Game edition, language, expansions, mods, and patches.
- Reproduction steps, actual and expected results, and a useful save or logs where needed. Please don't upload game executables or game-data archives.

You can report an Exult crash or regression without testing the original. Distinguish observations from assumptions and say what you haven't checked.

For a claimed difference from the original, test a legitimate copy and record its version, execution environment, and comparable setups and actions in both engines. Saves aren't interchangeable between the original game and Exult. Repeat tests when timing or randomness matters, and make sure observations support any proposed formulas or constants. Explain the effect on play: some differences are intentional enhancements or fixes.

A decompilation or AI comparison can suggest something to investigate, but a compatibility report must stand on results reproduced by running the games. Briefly say what prompted the investigation and who tested it, keeping any source disclosure within the sharing restrictions above. Mark unconfirmed claims as unverified; compatibility patches need verified behavior as their basis.

## Writing fixes and using AI

Develop fixes from Exult's code and documentation, independently verified behavior, and references such as game manuals or independently researched data-format documentation. Explain the basis for non-obvious algorithms, tables, constants, and test expectations.

Confirming a behavior doesn't establish that an accompanying patch was written independently. If you've seen relevant original engine code, a decompilation, or an analysis of it, tell the team in general terms before implementing, reviewing, or merging an affected fix. When a reporter or contributor has seen that material, the team should normally have someone who hasn't seen it verify the behavior through runtime tests before accepting a compatibility change. The team can assess the particular change, request more evidence, ask another contributor to implement or review it, or decline the patch. This doesn't exclude you from contributing elsewhere.

AI assistance is welcome under the same guidelines, including the source restrictions above. Please don't supply restricted material to AI, direct an agent or browsing tool to it, ask it to retrieve or reconstruct that material, or submit output made that way. If AI did a substantial part of a report, patch, or test, explain its role and what you checked yourself. Review every change and verify claims before submitting them; avoid bulk lists of unchecked comparisons. This policy covers the inputs and references deliberately used for your work; it doesn't ask you to certify a model's training data.

## Sending a pull request

Try to keep each pull request focused on a small set of related changes. Separate pull requests for different features or unrelated fixes are usually easier to review and accept. Explain the problem, your approach, and the result. For behavior changes, include the evidence and source disclosures described above, and explain how you developed the fix and tests.

- Follow the surrounding code and [.clang-format](.clang-format), avoiding unrelated formatting changes.
- Use [INSTALL](INSTALL), [README.windows](README.windows), or [README.macOS](README.macOS) for build guidance.
- Build affected components, test the reported case and nearby behavior, and add a regression test where practical or provide manual test steps. Report results and anything you couldn't check, and follow up on relevant automated checks.
- Preserve copyright and license notices. Identify reused material and its license; see [COPYING](COPYING) and notices in affected files.

## When in doubt

Ask the maintainers about uncertain sources in general terms before sharing material or using an affected patch. If unsuitable material has already been posted, stop sharing or using it and flag the Exult comment or contribution without repeating its contents. The team may remove it, pause review, or request a fresh behavioral report. Removing material limits further sharing but doesn't resolve how an affected patch was developed; work that out with the team before continuing.

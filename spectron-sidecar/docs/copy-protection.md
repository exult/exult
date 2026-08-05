# Ultima VII copy protection (Black Gate)

Black Gate and Forge of Virtue gate progress with questions from **Finnigan** (mayor of Trinsic) and **Batlin** (Fellowship founder in Britain). Answers come from the bundled manuals: the map coordinates in the game manual, *The Book of Archaic Knowledge*, *The Book of Fellowship*, and *The Traveller's Companion*.

Spectron can use this page as a knowledge source when the Avatar faces copy-protection dialogue. These are **manual and geography facts**, not map coordinates or GPS-style cheats from the game engine.

## Finnigan's questions (map coordinates)

| Question | Answer |
| --- | --- |
| What is the latitude of the northernmost point of the island Spektran? | **120** |
| What longitude runs through the centre of the island Buccaneer's Den? | **60** |
| What longitude runs through the centre of the island Terfin? | **120** |
| What latitude runs through the centre of Dagger Isle? | **0** |
| What latitude runs through the centre of Skara Brae? | **30** |
| What latitude runs through the centre of the Deep Forest? | **60** |
| What latitude runs through the centre of Buccaneer's Den? | **60** |
| What longitude runs through the centre of Skara Brae? | **60** |

## Batlin's questions (manual facts)

| Question | Answer | Source |
| --- | --- | --- |
| According to the Book of Archaic Knowledge, how many times must ginseng be reboiled in order for it to be properly used as a magical reagent? | **40** | Book of Archaic Knowledge |
| How many runes are in the archaic script of the outdated Britannian language? | **31** | Book of Archaic Knowledge |
| According to the Book of Archaic Knowledge, how many places may the mandrake root naturally be found? | **2** | Book of Archaic Knowledge |
| In the Book of Fellowship, how many bandits can be seen surrounding the old man in the illustration on page three? | **6** | Book of Fellowship |
| According to the Traveller's Companion, how many parts of the body should one wish to protect with armour? | **6** | Traveller's Companion |
| According to the Book of Archaic Knowledge, fewer than how many pearls in 10,000 are black? | **1** | Book of Archaic Knowledge |
| On what page of the Book of Archaic Knowledge is the spell known as An Zu? | **42** | Book of Archaic Knowledge |

## In-game consequences

If the Avatar answers incorrectly, Exult sets a global failure flag (`failed_copy_protect`). While that flag is active, certain UI and gameplay checks behave as if the Avatar were confused (see Exult's `Game_window::failed_copy_protection()`).

## References

Public catalogues of these questions include the [Ultima Codex](https://wiki.ultimacodex.com/wiki/Ultima_VII_copy_protection) and Origin's original design notes for copy-protection dialogue.

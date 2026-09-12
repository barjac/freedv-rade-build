# Using freedv-rade-update

This is a guide to `freedv-rade-update`, the companion to freedv-rade-build.
It keeps an existing FreeDV RADE install (originally created by `freedv-rade-build`) up to
date, rebuilds it, or backs it up.

If you just installed FreeDV via `freedv-rade-build` and want to keep it in sync
with the latest code, the
**Basic usage** section below is all you need. If you're comfortable with Git
and want to test an upstream Pull Request, a patch, or a specific commit
before it's merged, skip ahead to **Advanced usage**.

Run it from the `Update-RADE` desktop icon, or from a terminal with
`~/freedv-rade-build/freedv-rade-update`.

## Basic usage

Every time it starts, the script first checks itself for updates and quietly
pulls any it finds — you never need to update `freedv-rade-update` or `freedv-rade-build` by hand.

You'll then see a short menu:

```
Do you want to (U)pdate, (F)ull re-build, (B)ackup, (R)estore from backup,
(S)witch backup/current, Create (D)esktop launcher or (Q)uit?
```

- **(U)pdate** — the normal choice. Pulls the latest FreeDV source and
  rebuilds only what's changed, which is much faster than a full rebuild.
- **(F)ull re-build** — starts completely fresh (runs `freedv-rade-build`
  again). Use this if an Update ever leaves things in a broken state — it
  happens occasionally when an underlying dependency (Opus, RADE) has moved
  in a way an incremental update can't cleanly follow. It won't touch your
  settings, start scripts, or desktop icons.
- **(B)ackup** — copies your current working install aside, so you can get
  back to it later.
- **(R)estore from backup** / **(S)witch backup/current** — only shown once a
  backup exists; restore replaces the current install with the backup,
  switch swaps the two around.
- **(D)esktop launcher** — (re)creates the desktop icon.

Choosing **(U)pdate** asks one more question first:

```
Update to current master branch now? [Y/n]
```

Just press **Enter** here (or type `Y` + **Enter**) and it will fetch the latest master
branch and rebuild — no further questions asked, other than the final backup
prompt and a summary you confirm before anything actually happens. This is
the right choice if you just want to stay in sync with the current master development
branch and have no reason to test anything unusual.

Answering **n** instead leads into the more advanced questions below — you
only need to go there if you specifically want to build a specific release
rather than the current master branch, test a Pull Request or a patch, or
(once you've used those) re-use a combination you saved earlier.

**Nothing is ever done until you see a final summary and confirm it** — you
can always back out at that point (answer `n`) with nothing changed.

If anything goes wrong, the whole run is logged to
`~/freedv-rade-build/freedv-rade-update.log` — attach that if you open a
GitHub issue.

---

## Advanced usage

Everything below only appears if you answer **n** to "Update to current
master branch now?", and is aimed at testing something other than the
current released master branch — a Pull Request someone's asked you to try,
your own patch, or a specific commit.

### Choosing a base: branch, tag, or Pull Request(s)

```
Use a specific branch, tag or PR(s)?
```

Answering **y** lets you type one or more of the following, space-separated:

- **A PR number** (e.g. `1485`) — fetches that Pull Request and merges it
  onto the base (master, unless you also give a branch/tag — see below).
  You can give several PR numbers to combine more than one at once, e.g.
  `1464 1472` builds master with both PRs merged in together.
- **A branch or tag name** (e.g. `v3.0-dev`) — used as the base instead of
  master. At most one entry can be a base like this; everything else is
  treated as something to merge *onto* it.
- **`branch:name`** — merges an extra branch on top of the base, the same
  way a PR number does, for a branch that isn't itself a PR (e.g. someone's
  work-in-progress branch you've been asked to test alongside a specific
  base).

For example, `v3.0-dev 1464 branch:some-persons-branch` builds `v3.0-dev`,
then merges PR #1464 and that extra branch on top of it, all in one run.

If a PR's own GitHub base differs from what you're building against, you'll
get a warning explaining the mismatch before anything happens — you can
merge anyway, skip that one PR, or abort.

### Patches

```
Add patch(es)?
```

Patches are `.patch` files kept in `~/freedv-rade-build/patches/`. Answering
**y** shows you a numbered list to pick from (space-separated numbers), with
`0` for typing a filename by hand if you have one somewhere else.

If you've used patches recently, you'll instead first be offered:

```
Recent patches:
   1) some-patch.patch
(U)se these as-is, (E)dit the list, or start (F)resh?
```

- **U** uses exactly the same patches as last time.
- **E** lets you adjust that same list — type numbers to remove
  (e.g. `1`), or add `a` to also pick something new from the full
  `patches/` directory (e.g. `1 a` removes #1 *and* offers to add more).
  Leave blank to keep everything as-is.
- **F** starts fresh from the full `patches/` listing.

Patches are applied in the order they were last modified (oldest first), so
a newer, more evolved patch always lands on top of one it might depend on.

**Commit pinning**: some patches only apply cleanly against a specific
commit of whatever they're based on, not the current tip (a PR keeps
moving after the patch was written, for instance). If a patch has a
matching `<patchname>.patch.commit` file next to it in `patches/`
(containing just a commit hash), the script automatically pins the build to
that commit before applying the patch — you don't need to do anything extra.
You can also pin to a specific commit yourself at the next question if
needed:

```
Use a specific commit?
```

(skipped automatically if a patch's own `.commit` sidecar already resolved
one for you).

### Saving and reusing combinations as profiles

Once you've built something with a particular branch/PR/patch combination,
you'll be asked:

```
Save this build configuration as a reusable profile?
```

Give it a short name and it's remembered under `~/freedv-rade-build/.profiles`
— you won't be asked this if nothing was actually customised (a profile used
exactly as saved, or a plain master build with nothing changed).

Next time you run an Update, if any profiles exist you'll see them listed
before the branch/PR question:

```
Saved profiles:
   1) some profile description
   2) another one
Profile number to use, or blank/N for none:
```

Pick a number, then choose:

```
(U)se '<description>' as-is, or (E)dit it first?
```

- **U** rebuilds exactly what was saved, no further questions.
- **E** walks you through the branch/PR, patch, and commit questions as
  normal, but each one is pre-filled from that profile's saved values —
  change whatever you want, keep the rest, and optionally save the result
  under a new name at the end.

Leaving it blank (or `N`) ignores profiles entirely and goes straight to
manual entry, same as if none existed.

### `--profile=NAME` (non-interactive)

If you'd rather skip the interactive menu entirely and jump straight to a
saved profile, run:

```
freedv-rade-update --profile="exact profile description"
```

This still stops at the final summary for confirmation, same as everything
else — it just answers all the earlier questions for you.

### `--dev` and personal fork branches

This is a hidden flag, mostly relevant if the maintainer has specifically
asked you to test a branch on their personal fork rather than an open PR:

```
freedv-rade-update --dev
```

This unlocks a `fork:branchname` entry at the branch/PR question, building
directly from `barjac/freedv-gui` instead of requiring an open upstream PR.
You won't normally need this — if a saved profile already contains a
`fork:` entry (which could only have happened if it was created with
`--dev` in the first place), the script detects that automatically and
enables it for that run without needing the flag again.

---

## Please don't hand-edit these scripts

If you find a bug in `freedv-rade-update` (or `freedv-rade-build`) or want to
improve it, **please don't just patch your own local copy** to fix it.
Two reasons:

1. The script pulls its own updates from GitHub automatically on every run —
   a local edit you haven't committed can conflict with that and get in the
   way, or simply be silently overwritten later.
2. Nobody else benefits from a fix that only exists on your machine.

Instead, please contribute it properly:

1. Fork [barjac/freedv-rade-build](https://github.com/barjac/freedv-rade-build)
   on GitHub.
2. Make your change there and test it.
3. Open a Pull Request against the main repository.

If you're not sure whether something's a bug or intentional, or want to
discuss an idea first, open a GitHub issue instead of diving straight into
code — much easier to sort out before any patching happens.

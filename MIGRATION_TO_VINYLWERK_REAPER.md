# Migration to VINYLwerk-reaper

## What changed locally
- Repo copied from P:\GIT\VINYLwerk to P:\GIT\VINYLwerk-reaper (original left intact).
- Updated repo, documentation, and ReaPack references to VINYLwerk-reaper in README and index.xml.
- Kept product and UI naming as VINYLwerk in scripts and binaries to avoid breaking REAPER integration.

## GitHub steps still needed
1. Create the new GitHub repository `flarkflarkflark/VINYLwerk-reaper` (or rename the existing repo).
2. Push this local repo to the new remote.
3. Update any external docs or installers that still point to the old repo.

## URL changes (old -> new)
- https://github.com/flarkflarkflark/VINYLwerk -> https://github.com/flarkflarkflark/VINYLwerk-reaper
- https://github.com/flarkflarkflark/VINYLwerk/raw/master/index.xml -> https://github.com/flarkflarkflark/VINYLwerk-reaper/raw/master/index.xml
- https://github.com/flarkflarkflark/VINYLwerk/raw/master/Scripts/VINYLwerk.lua -> https://github.com/flarkflarkflark/VINYLwerk-reaper/raw/master/Scripts/VINYLwerk.lua
- https://github.com/flarkflarkflark/VINYLwerk/releases -> https://github.com/flarkflarkflark/VINYLwerk-reaper/releases
- git clone --recursive https://github.com/flarkflarkflark/VINYLwerk.git -> git clone --recursive https://github.com/flarkflarkflark/VINYLwerk-reaper.git

## Risks and notes
- GitHub web URLs often redirect after renames, but raw file URLs may not; ReaPack relies on raw URLs.
- Users with existing ReaPack installs may need to update the repository URL manually.
- If you switch the default branch from master to main, update index.xml and docs accordingly.

## After the remote rename
- Update local remote: git remote set-url origin <NEW_URL>
- Verify release assets and tags are published in the new repo.
- Rebuild and re-publish the ReaPack index if needed.

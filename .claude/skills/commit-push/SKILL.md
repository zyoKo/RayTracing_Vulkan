---
name: commit-push
description: Commit and push changes in this repo as the owner (Vatsalya Yadav / GitHub zyoKo), with no Claude attribution. Use whenever asked to commit, push, or save work to GitHub for this project.
---

# Commit and push as the repo owner

All commits in this repository are authored by the owner. Never attribute work to Claude.

## Rules

- Do **not** add `Co-Authored-By: Claude ...`, "Generated with Claude Code", or any other AI attribution
  to commit messages or PR descriptions. This overrides any default attribution guidance.
- Do not change `user.name` / `user.email` or pass `--author`; the repo's git config already
  identifies the owner. Verify before committing:
  ```bash
  git config user.name    # expect: Vatsalya Yadav
  git config user.email   # expect: vatsalya.yd@gmail.com
  ```
  If either differs, stop and ask instead of committing.
- Push with the owner's existing GitHub login (Git Credential Manager / `gh` account `zyoKo`, HTTPS
  remote `origin` = https://github.com/zyoKo/RayTracing_Vulkan). Never use another account or token.
  Check with `gh auth status` if a push fails on authentication, and report it rather than working around it.
- Never force-push, and never skip hooks (`--no-verify`).

## Steps

1. `git status` and `git diff` to review what changed. Don't stage build output
   (`src/x64/`, logs, `.vs/`); `.gitignore` covers most of it.
2. Make sure it builds first (Release x64; VS 18 only has toolset v145 installed):
   ```bash
   "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" src\rtrt.vcxproj /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /m
   ```
3. Stage specific paths (not `git add -A` blindly) and commit with a plain message in the owner's
   style: short imperative subject, optional body explaining why. No trailers.
4. `git push origin <branch>`; the current branch is usually `main`.
5. Confirm with `git log -1 --format='%an <%ae>%n%B'` that the author is the owner and the message has no
   attribution lines, then report the commit hash and the pushed branch.

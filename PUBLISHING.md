# Publish the prepared repository

The local repository uses `main` and contains the initial release commit.
The author identity was supplied by the project owner. GitHub CLI remains unauthenticated;
no remote repository or successful push is claimed.

Run these commands from this repository in PowerShell after putting Git and GitHub CLI on PATH:

```powershell
gh auth login
gh api user --jq .login
git status
git log -1 --format=fuller
gh repo create dakiemdarktharr/ascii-video-cpp --public --source=. --remote=origin --push
git rev-parse HEAD
gh repo view --json url --jq .url
gh run list --branch main
gh run watch --exit-status
gh repo view --web
```

Confirm that the authenticated account is `dakiemdarktharr` before creating the repository.
`gh repo create` fails if a repository with that name already exists. It does not overwrite it.
In that case inspect the existing repository before choosing a different name or connecting a remote.
There is no force push in this procedure. Do not continue past a failed push.

After the workflow completes, inspect README's GIF, poster and MP4 links in GitHub.
The workflow has not run remotely until the repository is pushed.

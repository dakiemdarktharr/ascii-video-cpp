# Repository and updates

The project is published at [dakiemdarktharr/ascii-video-cpp](https://github.com/dakiemdarktharr/ascii-video-cpp)
on `main`. The initial release commit is
`77f9216af8b772b03e76fe18e6f1f8ccb1770995`, authored with the identity supplied by the project owner.

For future updates, run the build and tests in README, inspect the changes, then use a normal push:

```powershell
git status
git diff
git add .
git diff --cached --check
git diff --cached
git commit -m "Describe the change"
git push origin main
gh run list --branch main
gh run watch --exit-status
```

Use `gh auth status` to inspect CLI authentication. If credentials are unavailable, run
`gh auth login` and confirm that `gh api user --jq .login` reports the intended account.
Never commit credentials or use force push for this workflow.

[GitHub Actions](https://github.com/dakiemdarktharr/ascii-video-cpp/actions/workflows/build.yml)
is the source for remote build status. After updates, inspect README's GIF, poster and MP4 links.

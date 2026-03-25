# Contributing to Gate-Lang
To practice professional git usage, we'll be using a simplified GitHub Flow for making changes, detailed here.

## The Golden Rules
- Never push directly to main
- Every code change needs an Issue. If you are writing code, there should be an open issue for it.
- Keep commits and PRs small

## How to Make a Change (Step-by-Step)
1. Claim an Issue
Find an unassigned issue on the GitHub Issues board and assign it to yourself. This prevents two people from doing the same work.

2. Update Your Local Repo
Start with the latest code merged. Run
```bash
git checkout main
git pull origin main
```

3. Running your code
You can build the CMake configuration with:
`cmake -S . -B build` 
and you can build the project with:
`cmake --build build`
and then run it with:
`./build/gate-lang`

4. Create a Branch
Create a new branch for your specific issue. Use a prefix like feature/, bugfix/, or docs/, followed by a short description and the issue number.
```bash
git checkout -b feature/peg-parser-rules-#4
```

5. Write Code and Commit
Write your code. Make sure it compiles. Commit your changes with clear messages.
```bash
git add .
git commit -m "Add PEG rules for HalfAdder syntax"
```

6. Push to GitHub
Push your new branch up to the remote repository.
```bash
git push -u origin feature/peg-parser-rules-#4
```

7. Open a Pull Request (PR)
Go to GitHub and open a Pull Request from your branch into main.

In the PR description, write Closes #4 (use your actual issue number). GitHub will automatically close the issue when the PR merges.

Wait for at least one teammate to review and approve your code.

8. Merge and Clean Up
Once approved, click the green "Merge" button. Afterward, delete your branch on GitHub and locally to keep things tidy.

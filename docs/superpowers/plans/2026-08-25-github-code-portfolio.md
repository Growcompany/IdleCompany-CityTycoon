# CompanyGrowthRenewal GitHub Code Portfolio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish a public, code-focused GitHub portfolio repository without exposing licensed assets, credentials, personal paths, or Azure-only history.

**Architecture:** Export an allowlisted snapshot from committed Git object `7df820f2469822c17f2ce1e5c60f4c46093a27dc` into a separate repository. Preserve all project-owned C++ source, then add curated documentation, tooling, sample data, and sanitized AI workflow files before a clean one-commit publication.

**Tech Stack:** Unreal Engine 5.4 C++, CommonUI, Enhanced Input, PlayFab, Firebase Functions TypeScript, PowerShell, Python, JavaScript, GitHub

**Spec:** `docs/superpowers/specs/2026-08-25-github-code-portfolio-design.md`

## Global Constraints

- Azure DevOps remains the private source of truth.
- Export only committed source from `7df820f2469822c17f2ce1e5c60f4c46093a27dc`; never copy the dirty working tree wholesale.
- Do not publish `Content`, third-party plugins, binaries, real deployment configuration, credentials, personal paths, local state, backups, or existing Git history.
- Keep all project-owned C++ source and C++ automation tests.
- The public repository starts on branch `main` with a clean history.
- The repository is public portfolio code, not a redistributable asset package or a complete runnable game build.

---

### Task 1: Create the allowlisted committed snapshot

**Files:**
- Create: `Source/CompanyGrowthRenewal/**`
- Create: `CompanyGrowthRenewal.uproject`
- Create: `DataImport/Samples/**`
- Create: `Tools/**`
- Create: `docs/Reference/**`

**Interfaces:**
- Consumes: committed Git tree `7df820f2469822c17f2ce1e5c60f4c46093a27dc`
- Produces: asset-free public source tree used by every later task

- [ ] **Step 1: Export `Source/CompanyGrowthRenewal` and the project manifest from the fixed commit**

Run from the private source repository with an explicit destination and verify the destination does not already contain a Git repository.

- [ ] **Step 2: Export representative data and tools**

Include three DataTable samples plus the allowlisted Balance, CheatDoc, MainMapPreview, and automation files. Exclude deployment helpers, credentials, binaries, and generated evidence.

- [ ] **Step 3: Export selected technical references**

Include architecture, input/camera, backend contract, CommonUI playbooks, reusable capability maps, rendering troubleshooting, and mobile optimization documents. Exclude game design specifications and private decision history.

- [ ] **Step 4: Prove asset exclusion**

Run:

```powershell
rg --files | rg '(?i)\.(uasset|umap|pak|exe|dll)$|(^|/)Content/|node_modules/'
```

Expected: no output.

### Task 2: Publish the AI workflow safely

**Files:**
- Create: `AGENTS.md`
- Create: `CLAUDE.md`
- Create: `.agents/skills/**`
- Create: `.claude/agents/**`
- Create: `.claude/commands/**`
- Create: `.claude/skills/**`
- Create: `.claude/hooks/**`
- Create: `.codex/agents/**`
- Create: `.codex/hooks/**`
- Create: `docs/AI_WORKFLOW.md`

**Interfaces:**
- Consumes: private repository AI rules and the publication exclusions in the spec
- Produces: executable examples of role separation, review gates, Skills, and Hooks without local state or credentials

- [ ] **Step 1: Copy the approved AI workflow files**

Exclude settings, state, backups, automatic Push commands, and remote-specific hooks.

- [ ] **Step 2: Replace local machine details**

Replace the private project path with `${PROJECT_ROOT}`, the engine path with `${UE_ROOT}`, and Azure account or remote text with neutral placeholders.

- [ ] **Step 3: Write `docs/AI_WORKFLOW.md`**

Document the developer-owned decision boundary and the flow `constraints → role routing → implementation → independent review → build/test/PIE/device evidence → accept or reject → guard automation`.

- [ ] **Step 4: Verify sanitization**

Run high-confidence secret, private-path, and remote URL scans. Expected result: zero matches.

### Task 3: Build the recruiter-facing navigation

**Files:**
- Create: `README.md`
- Create: `docs/ARCHITECTURE.md`
- Create: `docs/TROUBLESHOOTING.md`
- Create: `docs/PUBLICATION_NOTES.md`
- Create: `NOTICE.md`
- Create: `.gitignore`

**Interfaces:**
- Consumes: exported source paths from Tasks 1 and 2
- Produces: a browsable portfolio entry point with valid relative deep links

- [ ] **Step 1: Write the README overview and scope disclaimer**

State `2025.11 – present`, individual development, UE 5.4 C++, code-only portfolio scope, omitted licensed assets, and non-runnable-build limitations.

- [ ] **Step 2: Add eight technical deep-link sections**

Link Subsystems, DataTables, CommonUI, save/offline, input/camera, PlayFab/Firebase, tests/tools, and troubleshooting to actual repository files.

- [ ] **Step 3: Add architecture and troubleshooting documents**

Use Mermaid or text diagrams plus evidence-backed cases. Do not claim that every declared test passes or that unmeasured platforms were validated.

- [ ] **Step 4: Add ownership notice and ignore rules**

State that the repository grants no asset redistribution rights and excludes third-party content. Ignore Unreal-generated output, assets, deployment files, credentials, local state, and dependencies.

### Task 4: Verify the public snapshot

**Files:**
- Verify: all repository files

**Interfaces:**
- Consumes: complete staged public repository
- Produces: evidence that publication requirements are satisfied

- [ ] **Step 1: Compare C++ source inventory against the fixed commit**

Generate sorted SHA-256 manifests for the committed source and exported source. Expected: identical entries and hashes except the documented Firebase URL sanitization file.

- [ ] **Step 2: Validate every README relative link**

Extract Markdown targets and assert that every local target exists.

- [ ] **Step 3: Run safety scans**

Expected: zero high-confidence secrets, zero personal paths, zero Azure URLs, zero binary assets, zero `node_modules` entries.

- [ ] **Step 4: Run the published tool tests**

Run:

```powershell
node --test Tools/Balance/test/*.test.js
py -3 -m unittest discover -s Tools/MainMapPreview -p 'test_*.py'
```

Expected: both commands exit 0. Tests that require Unreal Editor or captured evidence must be documented and excluded from this standalone gate.

### Task 5: Create and publish the GitHub repository

**Files:**
- Create: local `.git/` metadata
- Create: GitHub repository `Growcompany/CompanyGrowthRenewal-Portfolio`

**Interfaces:**
- Consumes: verified public snapshot from Task 4
- Produces: public GitHub URL and immutable initial commit

- [ ] **Step 1: Initialize the clean repository**

Run:

```powershell
git init -b main
git add .
git commit -m "docs: publish CompanyGrowthRenewal code portfolio"
```

- [ ] **Step 2: Create the public GitHub repository**

Create `Growcompany/CompanyGrowthRenewal-Portfolio` with the description `UE5.4 C++ client architecture, automation, and AI-assisted engineering workflow for an idle city tycoon.`

- [ ] **Step 3: Push and verify GitHub content**

Confirm public visibility, default branch `main`, README rendering, and presence of every deep-linked file.

- [ ] **Step 4: Connect the Notion portfolio**

Set the project URL to the public GitHub repository and verify the public Notion page retains the `2025년 11월 ~ 진행 중` period and technical portfolio content.

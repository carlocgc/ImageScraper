# UI / Product Backlog

## Goal

Track the remaining user-facing work after the major panel extraction and media-preview upgrades.

## Priorities

### 1. Reddit account controls

Keep the current fast local sign-out as the default, but add a separate destructive action for full server-side revocation.

Why this is separate from sign-out:

- local sign-out is instant and keeps re-auth smooth
- Reddit token revocation introduces a multi-minute delay before sign-in works cleanly again
- users still need a way to revoke app access explicitly when they want the stronger security action

Recommended slice:

- add a distinct `Deauthorise App` action that calls `RevokeAccessTokenRequest`
- warn clearly about the expected re-auth delay before proceeding
- clear local tokens after a successful revoke

### 2. Field-level help in Download Options and Credentials

The core panels still rely too much on implicit knowledge.

Recommended slice:

- add terse hover text to each provider input and credential field
- support right-click copy of the relevant registration/help URL for credential fields
- keep the copy short enough that the panels stay readable

### 3. Reddit availability warning

The UI should say out loud what the README already implies: new Reddit app registrations are restricted, so the integration may not be usable for users without existing credentials.

Recommended slice:

- add a small inline warning under the Reddit provider controls
- keep it visible but unobtrusive
- avoid turning it into a modal or a blocker

### 4. Richer Downloads history

The current Downloads view is a filesystem-first browser. If it needs to become more provider-aware, the design should be intentional rather than bolting on more columns blindly.

Open questions:

- should filtering happen through provider tabs, a combo box, or a query bar
- should provider-specific context live in extra columns, badges, or a details pane
- what metadata needs to be persisted so history can show things like Reddit scope/sort or other run parameters

Recommended slice:

- decide the target interaction model first
- then add only the metadata needed to support that model cleanly

# v32opt changes

## 20261109-dev

* Dead variables: a `;@var loc=Rn` whose register is never written in the final region of the function becomes
  `optimized-out` (so v32sim shows `<optimized out>` instead of a stale register). New test `testing/vars/dead`.

## 20261105-dev

* Variable locations after register promotion (V32DBG): `;@var loc=BP+n` is rewritten to the register in
  the range it is promoted for (leaf, register and loop promotion), and the move is recorded with the new
  `;@home FROM TO` marker (understood by `v32as` 20261105-dev).
* Inlining no longer copies the callee's `;@var` / `;@scope` lines into the caller.
* `--check-vars`: verify the records afterwards (exit status 3).
* `testing/vars/` with `run.sh`; `testing/srcmarks/run.sh` skips it.
* Not done: marking variables with only dead stores `optimized-out`.
* `make version VERSION=20261105-dev` stamped; `install` and `sysinstall` unchanged (both present).

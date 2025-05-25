# Libraries

| Name                     | Description |
|--------------------------|-------------|
| *libziacoin_cli*         | RPC client functionality used by *ziacoin-cli* executable |
| *libziacoin_common*      | Home for common functionality shared by different executables and libraries. Similar to *libziacoin_util*, but higher-level (see [Dependencies](#dependencies)). |
| *libziacoin_consensus*   | Consensus functionality used by *libziacoin_node* and *libziacoin_wallet*. |
| *libziacoin_crypto*      | Hardware-optimized functions for data encryption, hashing, message authentication, and key derivation. |
| *libziacoin_kernel*      | Consensus engine and support library used for validation by *libziacoin_node*. |
| *libziacoinqt*           | GUI functionality used by *ziacoin-qt* and *ziacoin-gui* executables. |
| *libziacoin_ipc*         | IPC functionality used by *ziacoin-node*, *ziacoin-wallet*, *ziacoin-gui* executables to communicate when [`-DENABLE_IPC=ON`](multiprocess.md) is used. |
| *libziacoin_node*        | P2P and RPC server functionality used by *ziacoind* and *ziacoin-qt* executables. |
| *libziacoin_util*        | Home for common functionality shared by different executables and libraries. Similar to *libziacoin_common*, but lower-level (see [Dependencies](#dependencies)). |
| *libziacoin_wallet*      | Wallet functionality used by *ziacoind* and *ziacoin-wallet* executables. |
| *libziacoin_wallet_tool* | Lower-level wallet functionality used by *ziacoin-wallet* executable. |
| *libziacoin_zmq*         | [ZeroMQ](../zmq.md) functionality used by *ziacoind* and *ziacoin-qt* executables. |

## Conventions

- Most libraries are internal libraries and have APIs which are completely unstable! There are few or no restrictions on backwards compatibility or rules about external dependencies. An exception is *libziacoin_kernel*, which, at some future point, will have a documented external interface.

- Generally each library should have a corresponding source directory and namespace. Source code organization is a work in progress, so it is true that some namespaces are applied inconsistently, and if you look at [`add_library(ziacoin_* ...)`](../../src/CMakeLists.txt) lists you can see that many libraries pull in files from outside their source directory. But when working with libraries, it is good to follow a consistent pattern like:

  - *libziacoin_node* code lives in `src/node/` in the `node::` namespace
  - *libziacoin_wallet* code lives in `src/wallet/` in the `wallet::` namespace
  - *libziacoin_ipc* code lives in `src/ipc/` in the `ipc::` namespace
  - *libziacoin_util* code lives in `src/util/` in the `util::` namespace
  - *libziacoin_consensus* code lives in `src/consensus/` in the `Consensus::` namespace

## Dependencies

- Libraries should minimize what other libraries they depend on, and only reference symbols following the arrows shown in the dependency graph below:

<table><tr><td>

```mermaid

%%{ init : { "flowchart" : { "curve" : "basis" }}}%%

graph TD;

ziacoin-cli[ziacoin-cli]-->libziacoin_cli;

ziacoind[ziacoind]-->libziacoin_node;
ziacoind[ziacoind]-->libziacoin_wallet;

ziacoin-qt[ziacoin-qt]-->libziacoin_node;
ziacoin-qt[ziacoin-qt]-->libziacoinqt;
ziacoin-qt[ziacoin-qt]-->libziacoin_wallet;

ziacoin-wallet[ziacoin-wallet]-->libziacoin_wallet;
ziacoin-wallet[ziacoin-wallet]-->libziacoin_wallet_tool;

libziacoin_cli-->libziacoin_util;
libziacoin_cli-->libziacoin_common;

libziacoin_consensus-->libziacoin_crypto;

libziacoin_common-->libziacoin_consensus;
libziacoin_common-->libziacoin_crypto;
libziacoin_common-->libziacoin_util;

libziacoin_kernel-->libziacoin_consensus;
libziacoin_kernel-->libziacoin_crypto;
libziacoin_kernel-->libziacoin_util;

libziacoin_node-->libziacoin_consensus;
libziacoin_node-->libziacoin_crypto;
libziacoin_node-->libziacoin_kernel;
libziacoin_node-->libziacoin_common;
libziacoin_node-->libziacoin_util;

libziacoinqt-->libziacoin_common;
libziacoinqt-->libziacoin_util;

libziacoin_util-->libziacoin_crypto;

libziacoin_wallet-->libziacoin_common;
libziacoin_wallet-->libziacoin_crypto;
libziacoin_wallet-->libziacoin_util;

libziacoin_wallet_tool-->libziacoin_wallet;
libziacoin_wallet_tool-->libziacoin_util;

classDef bold stroke-width:2px, font-weight:bold, font-size: smaller;
class ziacoin-qt,ziacoind,ziacoin-cli,ziacoin-wallet bold
```
</td></tr><tr><td>

**Dependency graph**. Arrows show linker symbol dependencies. *Crypto* lib depends on nothing. *Util* lib is depended on by everything. *Kernel* lib depends only on consensus, crypto, and util.

</td></tr></table>

- The graph shows what _linker symbols_ (functions and variables) from each library other libraries can call and reference directly, but it is not a call graph. For example, there is no arrow connecting *libziacoin_wallet* and *libziacoin_node* libraries, because these libraries are intended to be modular and not depend on each other's internal implementation details. But wallet code is still able to call node code indirectly through the `interfaces::Chain` abstract class in [`interfaces/chain.h`](../../src/interfaces/chain.h) and node code calls wallet code through the `interfaces::ChainClient` and `interfaces::Chain::Notifications` abstract classes in the same file. In general, defining abstract classes in [`src/interfaces/`](../../src/interfaces/) can be a convenient way of avoiding unwanted direct dependencies or circular dependencies between libraries.

- *libziacoin_crypto* should be a standalone dependency that any library can depend on, and it should not depend on any other libraries itself.

- *libziacoin_consensus* should only depend on *libziacoin_crypto*, and all other libraries besides *libziacoin_crypto* should be allowed to depend on it.

- *libziacoin_util* should be a standalone dependency that any library can depend on, and it should not depend on other libraries except *libziacoin_crypto*. It provides basic utilities that fill in gaps in the C++ standard library and provide lightweight abstractions over platform-specific features. Since the util library is distributed with the kernel and is usable by kernel applications, it shouldn't contain functions that external code shouldn't call, like higher level code targeted at the node or wallet. (*libziacoin_common* is a better place for higher level code, or code that is meant to be used by internal applications only.)

- *libziacoin_common* is a home for miscellaneous shared code used by different ZiaCoin Core applications. It should not depend on anything other than *libziacoin_util*, *libziacoin_consensus*, and *libziacoin_crypto*.

- *libziacoin_kernel* should only depend on *libziacoin_util*, *libziacoin_consensus*, and *libziacoin_crypto*.

- The only thing that should depend on *libziacoin_kernel* internally should be *libziacoin_node*. GUI and wallet libraries *libziacoinqt* and *libziacoin_wallet* in particular should not depend on *libziacoin_kernel* and the unneeded functionality it would pull in, like block validation. To the extent that GUI and wallet code need scripting and signing functionality, they should be able to get it from *libziacoin_consensus*, *libziacoin_common*, *libziacoin_crypto*, and *libziacoin_util*, instead of *libziacoin_kernel*.

- GUI, node, and wallet code internal implementations should all be independent of each other, and the *libziacoinqt*, *libziacoin_node*, *libziacoin_wallet* libraries should never reference each other's symbols. They should only call each other through [`src/interfaces/`](../../src/interfaces/) abstract interfaces.

## Work in progress

- Validation code is moving from *libziacoin_node* to *libziacoin_kernel* as part of [The libziacoinkernel Project #27587](https://github.com/ziacoin/ziacoin/issues/27587)

# Moon Building System

建筑系统的单一规范入口已经收敛到 [assets/building/README.md](../../assets/building/README.md)。

这个目录不再维护独立设计文档，避免规范与实现说明重复演化。

仅保留一条模块边界约束：

1. `BuildingDefinition` 是内部 resolved geometry，不是对外 semantic schema。

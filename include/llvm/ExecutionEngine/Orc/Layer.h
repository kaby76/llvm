//===---------------- Layer.h -- Layer interfaces --------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Layer interfaces.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_EXECUTIONENGINE_ORC_LAYER_H
#define LLVM_EXECUTIONENGINE_ORC_LAYER_H

#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/MemoryBuffer.h"

namespace llvm {
namespace orc {

/// Interface for layers that accept LLVM IR.
class IRLayer {
public:
  IRLayer(ExecutionSession &ES);
  virtual ~IRLayer();

  /// Returns the ExecutionSession for this layer.
  ExecutionSession &getExecutionSession() { return ES; }

  /// Adds a MaterializationUnit representing the given IR to the given
  /// JITDylib.
  virtual Error add(JITDylib &JD, VModuleKey K, std::unique_ptr<Module> M);

  /// Adds a MaterializationUnit representing the given IR to the main
  /// JITDylib.
  Error add(VModuleKey K, std::unique_ptr<Module> M) {
    return add(ES.getMainJITDylib(), K, std::move(M));
  }

  /// Emit should materialize the given IR.
  virtual void emit(MaterializationResponsibility R, VModuleKey K,
                    std::unique_ptr<Module> M) = 0;

private:
  ExecutionSession &ES;
};

/// IRMaterializationUnit is a convenient base class for MaterializationUnits
/// wrapping LLVM IR. Represents materialization responsibility for all symbols
/// in the given module. If symbols are overridden by other definitions, then
/// their linkage is changed to available-externally.
class IRMaterializationUnit : public MaterializationUnit {
public:
  using SymbolNameToDefinitionMap = std::map<SymbolStringPtr, GlobalValue *>;

  /// Create an IRMaterializationLayer. Scans the module to build the
  /// SymbolFlags and SymbolToDefinition maps.
  IRMaterializationUnit(ExecutionSession &ES, std::unique_ptr<Module> M);

  /// Create an IRMaterializationLayer from a module, and pre-existing
  /// SymbolFlags and SymbolToDefinition maps. The maps must provide
  /// entries for each definition in M.
  /// This constructor is useful for delegating work from one
  /// IRMaterializationUnit to another.
  IRMaterializationUnit(std::unique_ptr<Module> M, SymbolFlagsMap SymbolFlags,
                        SymbolNameToDefinitionMap SymbolToDefinition);

protected:
  std::unique_ptr<Module> M;
  SymbolNameToDefinitionMap SymbolToDefinition;

private:
  void discard(const JITDylib &JD, SymbolStringPtr Name) override;
};

/// MaterializationUnit that materializes modules by calling the 'emit' method
/// on the given IRLayer.
class BasicIRLayerMaterializationUnit : public IRMaterializationUnit {
public:
  BasicIRLayerMaterializationUnit(IRLayer &L, VModuleKey K,
                                  std::unique_ptr<Module> M);
private:

  void materialize(MaterializationResponsibility R) override;

  IRLayer &L;
  VModuleKey K;
};

/// Interface for Layers that accept object files.
class ObjectLayer {
public:
  ObjectLayer(ExecutionSession &ES);
  virtual ~ObjectLayer();

  /// Returns the execution session for this layer.
  ExecutionSession &getExecutionSession() { return ES; }

  /// Adds a MaterializationUnit representing the given IR to the given
  /// JITDylib.
  virtual Error add(JITDylib &JD, VModuleKey K, std::unique_ptr<MemoryBuffer> O);

  /// Adds a MaterializationUnit representing the given object to the main
  /// JITDylib.
  Error add(VModuleKey K, std::unique_ptr<MemoryBuffer> O) {
    return add(ES.getMainJITDylib(), K, std::move(O));
  }

  /// Emit should materialize the given IR.
  virtual void emit(MaterializationResponsibility R, VModuleKey K,
                    std::unique_ptr<MemoryBuffer> O) = 0;

private:
  ExecutionSession &ES;
};

/// Materializes the given object file (represented by a MemoryBuffer
/// instance) by calling 'emit' on the given ObjectLayer.
class BasicObjectLayerMaterializationUnit : public MaterializationUnit {
public:
  static Expected<std::unique_ptr<BasicObjectLayerMaterializationUnit>>
  Create(ObjectLayer &L, VModuleKey K, std::unique_ptr<MemoryBuffer> O);

  BasicObjectLayerMaterializationUnit(ObjectLayer &L, VModuleKey K,
                                      std::unique_ptr<MemoryBuffer> O,
                                      SymbolFlagsMap SymbolFlags);

private:

  void materialize(MaterializationResponsibility R) override;
  void discard(const JITDylib &JD, SymbolStringPtr Name) override;

  ObjectLayer &L;
  VModuleKey K;
  std::unique_ptr<MemoryBuffer> O;
};

/// Returns a SymbolFlagsMap for the object file represented by the given
/// buffer, or an error if the buffer does not contain a valid object file.
// FIXME: Maybe move to Core.h?
Expected<SymbolFlagsMap> getObjectSymbolFlags(ExecutionSession &ES,
                                              MemoryBufferRef ObjBuffer);

} // End namespace orc
} // End namespace llvm

#endif // LLVM_EXECUTIONENGINE_ORC_LAYER_H

#include <cmath>
#include "coreir_primitives.hpp"
#include "utils.hpp"

#include <coreir/ir/namespace.h>
#include <coreir/ir/value.h>

#include <jitsim/circuit.hpp>

namespace JITSim {

using namespace std;

Primitive BuildAdd(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sum = env.getIRBuilder().CreateAdd(lhs, rhs, "sum");
      
      return std::vector<llvm::Value *> { sum };
    }
  );
}

Primitive BuildSub(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *diff = env.getIRBuilder().CreateSub(lhs, rhs, "diff");
      
      return std::vector<llvm::Value *> { diff };
    }
  );
}

Primitive BuildMul(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *prod = env.getIRBuilder().CreateMul(lhs, rhs, "prod");

      return std::vector<llvm::Value *> { prod };
    }
  );
}      

Primitive BuildEq(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpEQ(lhs, rhs, "eq_comp");

      return std::vector<llvm::Value *> { comp };
    }
  );
}      
      
Primitive BuildNeq(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpNE(lhs, rhs, "neq_comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}      

Primitive BuildUGT(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpUGT(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildUGE(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpUGE(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildULT(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpULT(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildULE(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpULE(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildSGT(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpSGT(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildSGE(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpSGE(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildSLT(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpSLT(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildSLE(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpSLT(lhs, rhs, "comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildReg(CoreIR::Module *mod)
{
  int width = 0;
  for (const auto & val : mod->getGenArgs()) {
    if (val.first == "width") {
      width = val.second->get<int>();
    }
  }

  return Primitive(true, getNumBytes(width),
    { "in" }, {},
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[0], llvm::Type::getIntNPtrTy(env.getContext(), width));
      llvm::Value *output = env.getIRBuilder().CreateLoad(addr, "output");

      return std::vector<llvm::Value *> { output };
    },
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *input = args[0];
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[1], llvm::Type::getIntNPtrTy(env.getContext(), width));
      env.getIRBuilder().CreateStore(input, addr);
    }
  );
}

Primitive BuildMux(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sel = args[2];

      llvm::Value *if_cond =
        env.getIRBuilder().CreateICmpEQ(sel,
                                        llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 0)),
                                        "ifcond");

      llvm::Value *result =
        env.getIRBuilder().CreateSelect(if_cond, lhs, rhs, "result");

      return std::vector<llvm::Value *> { result };
    }
  );
}      
      
Primitive BuildMem(CoreIR::Module *mod)
{
  int width = 0; 
  unsigned depth = 0;

  for (const auto & val : mod->getGenArgs()) {
    if (val.first == "width") {
      width = val.second->get<int>();
    } else if (val.first == "depth") {
      depth = val.second->get<int>();
    }
  }

  return Primitive(true, getNumBytes(width*depth),
    { "waddr", "wdata", "wen" }, { "raddr" },
    [width, depth](auto &env, auto &args, auto &inst)
    {
      llvm::Value *raddr = args[0];
      llvm::Value *state_addr = args[1];

      // Check if raddr < depth
      llvm::Value *valid_cond =
        env.getIRBuilder().CreateICmpULT(raddr, 
                                         llvm::ConstantInt::get(env.getContext(), llvm::APInt(ceil(log2(depth)), 1)),
                                         "valid_cond");
      llvm::BasicBlock *then_bb = env.addBasicBlock("then", false);
      llvm::BasicBlock *else_bb = env.addBasicBlock("else", false);
      llvm::BasicBlock *merge_bb = env.addBasicBlock("merge", false);
      env.getIRBuilder().CreateCondBr(valid_cond, then_bb, else_bb);

      // Emit then block.
      env.setCurBasicBlock(then_bb);
      llvm::Value *cast_addr = 
        env.getIRBuilder().CreateBitCast(state_addr,
                                         llvm::Type::getIntNPtrTy(env.getContext(), width));

      /* Need to 0 extend this to the address width or llvm interprets it as negative */
      llvm::Value *full_addr = env.getIRBuilder().CreateZExt(raddr, llvm::Type::getInt64Ty(env.getContext()));

      llvm::Value *addr = env.getIRBuilder().CreateInBoundsGEP(cast_addr, full_addr, "addr");
      llvm::Value *rdata = env.getIRBuilder().CreateLoad(addr, "rdata");
      
      env.getIRBuilder().CreateBr(merge_bb);

      // Emit else block.
      env.setCurBasicBlock(else_bb);
      llvm::Value *else_val = llvm::ConstantInt::get(env.getContext(), llvm::APInt(width, 0));

      env.getIRBuilder().CreateBr(merge_bb);

      // Emit merge block.
      env.setCurBasicBlock(merge_bb);

      llvm::PHINode *phi_node =
        env.getIRBuilder().CreatePHI(llvm::Type::getIntNTy(env.getContext(), width), 2, "iftmp");
      phi_node->addIncoming(rdata, then_bb);
      phi_node->addIncoming(else_val, else_bb);

      return std::vector<llvm::Value *> { phi_node };
    },
    [width, depth](auto &env, auto &args, auto &inst)
    {
      llvm::Value *waddr = args[0];
      llvm::Value *wdata = args[1];
      llvm::Value *wen = args[2];
      llvm::Value *state_addr = args[3];

      // Check if waddr < depth
      llvm::Value *valid_cond =
        env.getIRBuilder().CreateICmpULT(waddr, 
                                         llvm::ConstantInt::get(env.getContext(), llvm::APInt(ceil(log2(depth)), 1)),
                                         "valid_cond");
      llvm::BasicBlock *valid_then_bb = env.addBasicBlock("valid_then", false);
      llvm::BasicBlock *valid_else_bb = env.addBasicBlock("valid_else", false);
      env.getIRBuilder().CreateCondBr(valid_cond, valid_then_bb, valid_else_bb);

      // Emit valid_then block.
      env.setCurBasicBlock(valid_then_bb);

      llvm::Value *cast_addr = 
        env.getIRBuilder().CreateBitCast(state_addr,
                                         llvm::Type::getIntNPtrTy(env.getContext(), width));

      llvm::Value *full_addr = env.getIRBuilder().CreateZExt(waddr, llvm::Type::getInt64Ty(env.getContext()));

      llvm::Value *addr = env.getIRBuilder().CreateInBoundsGEP(cast_addr, full_addr, "addr");

      llvm::Value *wen_cond =
        env.getIRBuilder().CreateICmpEQ(wen,
                                        llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 1)),
                                        "wen_cond");

      llvm::BasicBlock *wen_then_bb = env.addBasicBlock("wen_then", false);
      llvm::BasicBlock *wen_else_bb = env.addBasicBlock("wen_else", false);
      env.getIRBuilder().CreateCondBr(wen_cond, wen_then_bb, wen_else_bb);

      // Emit wen_then block.
      env.setCurBasicBlock(wen_then_bb);
      env.getIRBuilder().CreateStore(wdata, addr);
      env.getIRBuilder().CreateBr(wen_else_bb);

      env.setCurBasicBlock(wen_else_bb); // wen_else
      env.getIRBuilder().CreateBr(valid_else_bb); 

      env.setCurBasicBlock(valid_else_bb); // valid_else
    },
    [width, depth](uint8_t *state_ptr, const Instance &inst) {
      std::string str = inst.getArg("init");
      llvm::APInt init(width*depth, str, 2);
      int num_bytes = width*depth / 8;
      if (width*depth % 8 != 0) {
        num_bytes++;
      }

      memcpy(state_ptr, init.getRawData(), num_bytes);
    }
  );
}      

Primitive BuildLShr(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *value = args[0];
      llvm::Value *shift_amount = args[1];
      llvm::Value *result = env.getIRBuilder().CreateLShr(value, shift_amount, "shift_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildAShr(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *value = args[0];
      llvm::Value *shift_amount = args[1];
      llvm::Value *result = env.getIRBuilder().CreateAShr(value, shift_amount, "shift_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildShl(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *value = args[0];
      llvm::Value *shift_amount = args[1];
      llvm::Value *result = env.getIRBuilder().CreateShl(value, shift_amount, "shift_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildAnd(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *result = env.getIRBuilder().CreateAnd(lhs, rhs, "and_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildOr(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *result = env.getIRBuilder().CreateOr(lhs, rhs, "or_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildXor(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *result = env.getIRBuilder().CreateXor(lhs, rhs, "xor_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildNot(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *value = args[0];
      llvm::Value *result = env.getIRBuilder().CreateNot(value, "not_res");
      return std::vector<llvm::Value *> { result };
    }
  );
}

Primitive BuildOrr(CoreIR::Module *mod)
{
  return Primitive( 
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *value = args[0];
      llvm::Value *comp = env.getIRBuilder().CreateICmpNE(value,
                                                          llvm::ConstantInt::get(value->getType(), 0),
                                                          "orr_comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}

Primitive BuildZExt(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      std::string width_arg = inst.getArg("width_in");
      int width = std::stoi(width_arg);

      llvm::Value *value = args[0];
      llvm::Value *extended = env.getIRBuilder().CreateZExt(value,
        llvm::Type::getIntNTy(env.getContext(), width),
        "zext");

      return std::vector<llvm::Value *> { extended };
    }
  );
}

std::vector<llvm::Constant *> GetLUTInit(FunctionEnvironment &env, int bits, const Instance &inst)
{
  std::string str = inst.getArg("init");
  std::vector<llvm::Constant *> r(bits);
  for (unsigned i = 0; i < str.size(); i++) {
    if (str[i] == '1') {
      r[i] = llvm::ConstantInt::getTrue(env.getContext());
    } else {
      r[i] = llvm::ConstantInt::getFalse(env.getContext());
    }
  }

  return r;
}

Primitive BuildLUT(CoreIR::Module *mod)
{
  int N = 0;
  for (const auto & val : mod->getGenArgs()) {
    if (val.first == "N") {
      N = val.second->get<int>();
    }
  }

  int num_elems = 1 << N;

  return Primitive(
    [num_elems](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lookup = args[0];
      std::vector<llvm::Constant *> init = GetLUTInit(env, num_elems, inst);
      llvm::Type *boolty = llvm::Type::getInt1Ty(env.getContext());
      llvm::ArrayType *luttype = llvm::ArrayType::get(boolty, num_elems);
      llvm::Constant *lut = llvm::ConstantArray::get(luttype, init);
      llvm::GlobalVariable *lutvar = new llvm::GlobalVariable(*env.getModule().getModule(), luttype, true,
                                                              llvm::GlobalValue::PrivateLinkage,
                                                              lut);
      llvm::Value *addr = env.getIRBuilder().CreateInBoundsGEP(lutvar,
                            {llvm::ConstantInt::getFalse(env.getContext()), lookup}, "addr");
      llvm::Value *load = env.getIRBuilder().CreateLoad(addr, "load");

      return std::vector<llvm::Value *> { load };
    }
  );
}

static unordered_map<string,function<Primitive (CoreIR::Module *mod)>> InitializeMapping()
{
  unordered_map<string,function<Primitive (CoreIR::Module *mod)>> m;
  m["coreir.add"] = BuildAdd;
  m["coreir.sub"] = BuildSub;
  m["coreir.mul"] = BuildMul;

  m["coreir.eq"] = BuildEq;
  m["coreir.neq"] = BuildNeq;
  m["coreir.ugt"] = BuildUGT;
  m["coreir.uge"] = BuildUGE;
  m["coreir.ult"] = BuildULT;
  m["coreir.ule"] = BuildULE;
  m["coreir.sgt"] = BuildUGT;
  m["coreir.sge"] = BuildSGE;
  m["coreir.slt"] = BuildUGT;
  m["coreir.sle"] = BuildUGT;

  m["coreir.zext"] = BuildZExt;

  m["coreir.reg"] = BuildReg;
  m["coreir.mux"] = BuildMux;
  m["corebit.mux"] = BuildMux;
  m["coreir.mem"] = BuildMem;
  m["coreir.lshr"] = BuildLShr;
  m["coreir.ashr"] = BuildAShr;
  m["coreir.shl"] = BuildShl;

  m["coreir.and"] = BuildAnd;
  m["corebit.and"] = BuildAnd;
  m["coreir.or"] = BuildOr;
  m["corebit.or"] = BuildOr;
  m["coreir.xor"] = BuildXor;
  m["corebit.xor"] = BuildXor;
  m["coreir.not"] = BuildNot;
  m["corebit.not"] = BuildNot;
  m["coreir.orr"] = BuildOrr;

  m["commonlib.lutN"] = BuildLUT;

  return m;
}

Primitive BuildCoreIRPrimitive(CoreIR::Module *mod)
{
  static const unordered_map<string,function<Primitive (CoreIR::Module *mod)>> prim_map =
    InitializeMapping();
  string fullname = mod->getNamespace()->getName() + "." + mod->getName();

  if (prim_map.count(fullname) == 0) {
    cerr << "Unsupported primitive " << fullname << endl;
    assert(false);
  }

  auto iter = prim_map.find(fullname);
  assert(iter != prim_map.end());

  return iter->second(mod);
}

}

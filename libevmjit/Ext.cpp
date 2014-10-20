
#include "Ext.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/TypeBuilder.h>
#include <llvm/IR/IntrinsicInst.h>

#include <libdevcrypto/SHA3.h>

#include "Runtime.h"

using namespace llvm;

namespace dev
{
namespace eth
{
namespace jit
{

// TODO: Copy of fromAddress in VM.h
inline u256 fromAddress(Address _a)
{
	return (u160)_a;
}

struct ExtData 
{
	i256 address;
	i256 caller;
	i256 origin;
	i256 callvalue;
	i256 calldatasize;
	i256 gasprice;
	i256 prevhash;
	i256 coinbase;
	i256 timestamp;
	i256 number;
	i256 difficulty;
	i256 gaslimit;
	i256 codesize;
	const byte* calldata;
	const byte* code;
};

Ext::Ext(llvm::IRBuilder<>& _builder):
	CompilerHelper(_builder)
{
	auto&& ctx = _builder.getContext();
	auto module = getModule();

	auto i256Ty = m_builder.getIntNTy(256);
	auto i256PtrTy = i256Ty->getPointerTo();
	auto i8PtrTy = m_builder.getInt8PtrTy();

	m_args[0] = m_builder.CreateAlloca(i256Ty, nullptr, "ext.index");
	m_args[1] = m_builder.CreateAlloca(i256Ty, nullptr, "ext.value");
	m_arg2 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg2");
	m_arg3 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg3");
	m_arg4 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg4");
	m_arg5 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg5");
	m_arg6 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg6");
	m_arg7 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg7");
	m_arg8 = m_builder.CreateAlloca(i256Ty, nullptr, "ext.arg8");

	Type* elements[] = {
		i256Ty,	 // i256 address;
		i256Ty,	 // i256 caller;
		i256Ty,	 // i256 origin;
		i256Ty,	 // i256 callvalue;
		i256Ty,	 // i256 calldatasize;
		i256Ty,	 // i256 gasprice;
		i256Ty,	 // i256 prevhash;
		i256Ty,	 // i256 coinbase;
		i256Ty,	 // i256 timestamp;
		i256Ty,	 // i256 number;
		i256Ty,	 // i256 difficulty;
		i256Ty,	 // i256 gaslimit;
		i256Ty,  // i256 codesize
		i8PtrTy, // byte* calldata
		i8PtrTy, // byte* code

	};
	auto extDataTy = StructType::create(elements, "ext.Data");

	m_data = m_builder.CreateAlloca(extDataTy, nullptr, "ext.data");

	using llvm::types::i;
	using Linkage = llvm::GlobalValue::LinkageTypes;
	m_init = Function::Create(FunctionType::get(m_builder.getVoidTy(), extDataTy->getPointerTo(), false), Linkage::ExternalLinkage, "ext_init", module);
	m_store = Function::Create(TypeBuilder<void(i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_store", module);
	m_setStore = Function::Create(TypeBuilder<void(i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_setStore", module);
	m_calldataload = Function::Create(TypeBuilder<void(i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_calldataload", module);
	m_balance = Function::Create(TypeBuilder<void(i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_balance", module);
	m_suicide = Function::Create(TypeBuilder<void(i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_suicide", module);
	m_create = Function::Create(TypeBuilder<void(i<256>*, i<256>*, i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_create", module);
	Type* args[] = {i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy, i256PtrTy};
	m_call = Function::Create(FunctionType::get(m_builder.getVoidTy(), args, false), Linkage::ExternalLinkage, "ext_call", module);
	m_bswap = Intrinsic::getDeclaration(module, Intrinsic::bswap, i256Ty);
	m_sha3 = Function::Create(TypeBuilder<void(i<256>*, i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_sha3", module);
	m_exp = Function::Create(TypeBuilder<void(i<256>*, i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_exp", module);
	m_codeAt = Function::Create(TypeBuilder<i<8>*(i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_codeAt", module);
	m_codesizeAt = Function::Create(TypeBuilder<void(i<256>*, i<256>*), true>::get(ctx), Linkage::ExternalLinkage, "ext_codesizeAt", module);

	m_builder.CreateCall(m_init, m_data);
}

llvm::Value* Ext::store(llvm::Value* _index)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateCall(m_store, m_args);
	return m_builder.CreateLoad(m_args[1]);
}

void Ext::setStore(llvm::Value* _index, llvm::Value* _value)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateStore(_value, m_args[1]);
	m_builder.CreateCall(m_setStore, m_args);
}

Value* Ext::getDataElem(unsigned _index, const Twine& _name)
{
	auto valuePtr = m_builder.CreateStructGEP(m_data, _index, _name);
	return m_builder.CreateLoad(valuePtr);
}

Value* Ext::address() { return getDataElem(0, "address"); }
Value* Ext::caller() { return getDataElem(1, "caller"); }
Value* Ext::origin() { return getDataElem(2, "origin"); }
Value* Ext::callvalue() { return getDataElem(3, "callvalue"); }
Value* Ext::calldatasize() { return getDataElem(4, "calldatasize"); }
Value* Ext::gasprice() { return getDataElem(5, "gasprice"); }
Value* Ext::prevhash() { return getDataElem(6, "prevhash"); }
Value* Ext::coinbase() { return getDataElem(7, "coinbase"); }
Value* Ext::timestamp() { return getDataElem(8, "timestamp"); }
Value* Ext::number() { return getDataElem(9, "number"); }
Value* Ext::difficulty() { return getDataElem(10, "difficulty"); }
Value* Ext::gaslimit() { return getDataElem(11, "gaslimit"); }
Value* Ext::codesize() { return getDataElem(12, "codesize"); }
Value* Ext::calldata() { return getDataElem(13, "calldata"); }
Value* Ext::code() { return getDataElem(14, "code"); }

Value* Ext::calldataload(Value* _index)
{
	m_builder.CreateStore(_index, m_args[0]);
	m_builder.CreateCall(m_calldataload, m_args);
	return m_builder.CreateLoad(m_args[1]);
}

Value* Ext::bswap(Value* _value)
{
	return m_builder.CreateCall(m_bswap, _value);
}

Value* Ext::balance(Value* _address)
{
	auto address = bswap(_address); // to BE
	m_builder.CreateStore(address, m_args[0]);
	m_builder.CreateCall(m_balance, m_args);
	return m_builder.CreateLoad(m_args[1]);
}

void Ext::suicide(Value* _address)
{
	auto address = bswap(_address); // to BE
	m_builder.CreateStore(address, m_args[0]);
	m_builder.CreateCall(m_suicide, m_args[0]);
}

Value* Ext::create(llvm::Value* _endowment, llvm::Value* _initOff, llvm::Value* _initSize)
{
	m_builder.CreateStore(_endowment, m_args[0]);
	m_builder.CreateStore(_initOff, m_arg2);
	m_builder.CreateStore(_initSize, m_arg3);
	Value* args[] = {m_args[0], m_arg2, m_arg3, m_args[1]};
	m_builder.CreateCall(m_create, args);
	Value* address = m_builder.CreateLoad(m_args[1]);
	address = bswap(address); // to LE
	return address;
}

llvm::Value* Ext::call(llvm::Value*& _gas, llvm::Value* _receiveAddress, llvm::Value* _value, llvm::Value* _inOff, llvm::Value* _inSize, llvm::Value* _outOff, llvm::Value* _outSize, llvm::Value* _codeAddress)
{
	m_builder.CreateStore(_gas, m_args[0]);
	auto receiveAddress = bswap(_receiveAddress); // to BE
	m_builder.CreateStore(receiveAddress, m_arg2);
	m_builder.CreateStore(_value, m_arg3);
	m_builder.CreateStore(_inOff, m_arg4);
	m_builder.CreateStore(_inSize, m_arg5);
	m_builder.CreateStore(_outOff, m_arg6);
	m_builder.CreateStore(_outSize, m_arg7);
	auto codeAddress = bswap(_codeAddress); // toBE
	m_builder.CreateStore(codeAddress, m_arg8);

	llvm::Value* args[] = {m_args[0], m_arg2, m_arg3, m_arg4, m_arg5, m_arg6, m_arg7, m_arg8, m_args[1]};
	m_builder.CreateCall(m_call, args);
	_gas = m_builder.CreateLoad(m_args[0]); // Return gas
	return m_builder.CreateLoad(m_args[1]);
}

llvm::Value* Ext::sha3(llvm::Value* _inOff, llvm::Value* _inSize)
{
	m_builder.CreateStore(_inOff, m_args[0]);
	m_builder.CreateStore(_inSize, m_arg2);
	llvm::Value* args[] = {m_args[0], m_arg2, m_args[1]};
	m_builder.CreateCall(m_sha3, args);
	Value* hash = m_builder.CreateLoad(m_args[1]);
	hash = bswap(hash); // to LE
	return hash;
}

llvm::Value* Ext::exp(llvm::Value* _left, llvm::Value* _right)
{
	m_builder.CreateStore(_left, m_args[0]);
	m_builder.CreateStore(_right, m_arg2);
	llvm::Value* args[] = {m_args[0], m_arg2, m_args[1]};
	m_builder.CreateCall(m_exp, args);
	return m_builder.CreateLoad(m_args[1]);
}

llvm::Value* Ext::codeAt(llvm::Value* _addr)
{
	auto addr = bswap(_addr);
	m_builder.CreateStore(addr, m_args[0]);
	return m_builder.CreateCall(m_codeAt, m_args[0]);
}

llvm::Value* Ext::codesizeAt(llvm::Value* _addr)
{
	auto addr = bswap(_addr);
	m_builder.CreateStore(addr, m_args[0]);
	llvm::Value* args[] = {m_args[0], m_args[1]};
	m_builder.CreateCall(m_codesizeAt, args);
	return m_builder.CreateLoad(m_args[1]);
}

}


extern "C"
{

using namespace dev::eth::jit;

EXPORT void ext_init(ExtData* _extData)
{
	auto&& ext = Runtime::getExt();
	_extData->address = eth2llvm(fromAddress(ext.myAddress));
	_extData->caller = eth2llvm(fromAddress(ext.caller));
	_extData->origin = eth2llvm(fromAddress(ext.origin));
	_extData->callvalue = eth2llvm(ext.value);
	_extData->gasprice = eth2llvm(ext.gasPrice);
	_extData->calldatasize = eth2llvm(ext.data.size());
	_extData->prevhash = eth2llvm(ext.previousBlock.hash);
	_extData->coinbase = eth2llvm(fromAddress(ext.currentBlock.coinbaseAddress));
	_extData->timestamp = eth2llvm(ext.currentBlock.timestamp);
	_extData->number = eth2llvm(ext.currentBlock.number);
	_extData->difficulty = eth2llvm(ext.currentBlock.difficulty);
	_extData->gaslimit = eth2llvm(ext.currentBlock.gasLimit);
	_extData->codesize = eth2llvm(ext.code.size());
	_extData->calldata = ext.data.data();
	_extData->code = ext.code.data();
}

EXPORT void ext_store(i256* _index, i256* _value)
{
	auto index = llvm2eth(*_index);
	auto value = Runtime::getExt().store(index);
	*_value = eth2llvm(value);
}

EXPORT void ext_setStore(i256* _index, i256* _value)
{
	auto index = llvm2eth(*_index);
	auto value = llvm2eth(*_value);
	Runtime::getExt().setStore(index, value);
}

EXPORT void ext_calldataload(i256* _index, i256* _value)
{
	auto index = static_cast<size_t>(llvm2eth(*_index));
	assert(index + 31 > index); // TODO: Handle large index
	auto b = reinterpret_cast<byte*>(_value);
	for (size_t i = index, j = 31; i <= index + 31; ++i, --j)
		b[j] = i < Runtime::getExt().data.size() ? Runtime::getExt().data[i] : 0;
	// TODO: It all can be done by adding padding to data or by using min() algorithm without branch
}

EXPORT void ext_balance(h256* _address, i256* _value)
{
	auto u = Runtime::getExt().balance(right160(*_address));
	*_value = eth2llvm(u);
}

EXPORT void ext_suicide(h256* _address)
{
	Runtime::getExt().suicide(right160(*_address));
}

EXPORT void ext_create(i256* _endowment, i256* _initOff, i256* _initSize, h256* _address)
{
	auto&& ext = Runtime::getExt();
	auto endowment = llvm2eth(*_endowment);

	if (ext.balance(ext.myAddress) >= endowment)
	{
		ext.subBalance(endowment);
		u256 gas;	// TODO: Handle gas
		auto initOff = static_cast<size_t>(llvm2eth(*_initOff));
		auto initSize = static_cast<size_t>(llvm2eth(*_initSize));
		auto&& initRef = bytesConstRef(Runtime::getMemory().data() + initOff, initSize);
		auto&& onOp = bytesConstRef(); // TODO: Handle that thing
		h256 address = ext.create(endowment, &gas, initRef, onOp);
		*_address = address;
	}
	else
		*_address = {};
}



EXPORT void ext_call(i256* _gas, h256* _receiveAddress, i256* _value, i256* _inOff, i256* _inSize, i256* _outOff, i256* _outSize, h256* _codeAddress, i256* _ret)
{
	auto&& ext = Runtime::getExt();
	auto value = llvm2eth(*_value);

	auto ret = false;
	auto gas = llvm2eth(*_gas);
	if (ext.balance(ext.myAddress) >= value)
	{
		ext.subBalance(value);
		auto receiveAddress = right160(*_receiveAddress);
		auto inOff = static_cast<size_t>(llvm2eth(*_inOff));
		auto inSize = static_cast<size_t>(llvm2eth(*_inSize));
		auto outOff = static_cast<size_t>(llvm2eth(*_outOff));
		auto outSize = static_cast<size_t>(llvm2eth(*_outSize));
		auto&& inRef = bytesConstRef(Runtime::getMemory().data() + inOff, inSize);
		auto&& outRef = bytesConstRef(Runtime::getMemory().data() + outOff, outSize);
		OnOpFunc onOp{}; // TODO: Handle that thing
		auto codeAddress = right160(*_codeAddress);
		ret = ext.call(receiveAddress, value, inRef, &gas, outRef, onOp, {}, codeAddress);
	}

	*_gas = eth2llvm(gas);
	_ret->a = ret ? 1 : 0;
}

EXPORT void ext_sha3(i256* _inOff, i256* _inSize, i256* _ret)
{
	auto inOff = static_cast<size_t>(llvm2eth(*_inOff));
	auto inSize = static_cast<size_t>(llvm2eth(*_inSize));
	auto dataRef = bytesConstRef(Runtime::getMemory().data() + inOff, inSize);
	auto hash = sha3(dataRef);
	*_ret = *reinterpret_cast<i256*>(&hash);
}

EXPORT void ext_exp(i256* _left, i256* _right, i256* _ret)
{
	bigint left = llvm2eth(*_left);
	bigint right = llvm2eth(*_right);
	auto ret = static_cast<u256>(boost::multiprecision::powm(left, right, bigint(2) << 256));
	*_ret = eth2llvm(ret);
}

EXPORT unsigned char* ext_codeAt(h256* _addr256)
{
	auto&& ext = Runtime::getExt();
	auto addr = right160(*_addr256);
	auto& code = ext.codeAt(addr);
	return const_cast<unsigned char*>(code.data());
}

EXPORT void ext_codesizeAt(h256* _addr256, i256* _ret)
{
	auto&& ext = Runtime::getExt();
	auto addr = right160(*_addr256);
	auto& code = ext.codeAt(addr);
	*_ret = eth2llvm(u256(code.size()));
}

}
}
}

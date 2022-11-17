
#include "isitypes.h"

template<uint8_t BIT_SIZE>
struct memory_width {
	static constexpr uint32_t MASK = (1u << BIT_SIZE) - 1u;
	using mem_t = typename std::conditional<(BIT_SIZE < 9u), uint8_t,
		typename std::conditional<(BIT_SIZE < 17u), uint16_t, uint32_t>::type>::type;
};

template<uint8_t A_SIZE, uint8_t D_SIZE>
struct memory : public isiMemory {
	static constexpr uint32_t A_MASK = memory_width<A_SIZE>::MASK;
	static constexpr uint32_t D_MASK = memory_width<D_SIZE>::MASK;
	static constexpr uint32_t BLOCK_SIZE = (1U << A_SIZE);
	using mem_t = typename memory_width<D_SIZE>::mem_t;
	mem_t ram[BLOCK_SIZE];
	uint8_t sync[BLOCK_SIZE];
	virtual uint32_t x_rd(uint32_t a) {
		return this->ram[a & A_MASK];
	}
	virtual void x_wr(uint32_t a, uint32_t v) {
		a &= A_MASK;
		if(this->sync[a] & ISI_RAMCTL_RDONLY) return;
		v &= D_MASK;
		if(this->ram[a] != v) {
			if(this->sync[a] & ISI_RAMCTL_SYNC) this->info |= ISI_RAMINFO_SCAN;
		}
		this->ram[a] = v;
	}
	virtual uint32_t d_rd(uint32_t a) {
		return this->ram[a & A_MASK];
	}
	virtual uint32_t i_rd(uint32_t a) const {
		return this->ram[a & A_MASK];
	}
	virtual void i_wr(uint32_t a, uint32_t v) {
		a &= A_MASK;
		v &= D_MASK;
		if(this->ram[a] != v) {
			if(this->sync[a] & ISI_RAMCTL_SYNC) {
				this->info |= ISI_RAMINFO_SCAN;
				this->sync[a] |= ISI_RAMCTL_DELTA;
			}
		}
		this->ram[a] = v;
	}
	virtual uint32_t mask_addr(uint32_t a) const {
		return a & A_MASK;
	}
	virtual uint32_t mask_data(uint32_t v) const {
		return v & D_MASK;
	}
	virtual uint32_t byte_offset(uint32_t a) const {
		return (a & A_MASK) * sizeof(mem_t);
	}
	virtual uint8_t sync_rd(uint32_t a) {
		a &= A_MASK;
		uint8_t r = this->sync[a];
		this->sync[a] &= ~ISI_RAMCTL_DELTA;
		return r;
	}
	virtual void sync_set(uint32_t a, uint8_t sv) {
		this->sync[a & A_MASK] |= sv;
	}
	virtual void sync_clear(uint32_t a, uint8_t sv) {
		this->sync[a & A_MASK] &= ~sv;
	}
	virtual void d_wr(uint32_t a, uint32_t v) {
		a &= A_MASK;
		if(this->sync[a] & ISI_RAMCTL_RDONLY) return;
		v &= D_MASK;
		if(this->ram[a] != v) {
			if(this->sync[a] & ISI_RAMCTL_SYNC) this->info |= ISI_RAMINFO_SCAN;
		}
		this->ram[a] = v;
	}
	virtual void sync_wrblock(uint32_t a, uint32_t l, const uint8_t *src) {
		a &= A_MASK;
		uint8_t *write_ptr = (uint8_t*)(ram + a);
		uint8_t *limit_ptr = (uint8_t*)(ram + BLOCK_SIZE);
		while (l > 0) {
			*(write_ptr++) = *(src++);
			l--;
			if(write_ptr == limit_ptr) {
				write_ptr = (uint8_t*)ram;
			}
		}
	}
	virtual void sync_rdblock(uint32_t a, uint32_t l, uint8_t *dst) {
		a &= A_MASK;
		const uint8_t *read_ptr = (const uint8_t*)(ram + a);
		const uint8_t *limit_ptr = (const uint8_t*)(ram + BLOCK_SIZE);
		while (l > 0) {
			*(dst++) = *(read_ptr++);
			l--;
			if(read_ptr == limit_ptr) {
				read_ptr = (const uint8_t*)ram;
			}
		}
	}
	virtual bool isbrk(uint32_t a) const {
		return (this->sync[a & A_MASK] & ISI_RAMCTL_BREAK) != 0;
	}
	virtual void togglebrk(uint32_t a) {
		this->sync[a & A_MASK] ^= ISI_RAMCTL_BREAK;
	}

};

static isiClass<memory<16,16>> Mem6416_Con(ISIT_MEM16, "memory_64kx16", "64k x 16 Memory");

void Memory_Register()
{
	isi_register(&Mem6416_Con);
}





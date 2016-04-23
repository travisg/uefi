// Copyright 2016, Brian Swetland <swetland@frotz.net> 
//
// Permission is hereby granted, free of charge, to any person
// obtaining a copy of this software and associated documentation
// files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge,
// publish, distribute, sublicense, and/or sell copies of the Software,
// and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <efi.h>
#include <efilib.h>
#include <string.h>

#include "netifc.h"
#include "inet6.h"

static EFI_SIMPLE_NETWORK *snp;

#define MAX_FILTER 8
static mac_addr mcast_filters[MAX_FILTER];
static unsigned mcast_filter_count = 0;

#define NUM_BUFFER_PAGES	8
#define ETH_BUFFER_SIZE		1516
#define ETH_HEADER_SIZE		16
#define ETH_BUFFER_MAGIC	0x424201020304A7A7UL

typedef struct eth_buffer_t eth_buffer;
struct eth_buffer_t {
	uint64_t magic;
	eth_buffer *next;
	uint8_t data[0];
};

static EFI_PHYSICAL_ADDRESS eth_buffers_base = 0;
static eth_buffer *eth_buffers = NULL;

void *eth_get_buffer(size_t sz) {
	eth_buffer *buf;
	if (sz > ETH_BUFFER_SIZE) {
		return NULL;
	}
	if (eth_buffers == NULL) {
		return NULL;
	}
	buf = eth_buffers;
	eth_buffers = buf->next;
	buf->next = NULL;
	return buf->data;
}

void eth_put_buffer(void *data) {
	eth_buffer *buf = (void*) (((uint64_t) data) & (~2047));

	if (buf->magic != ETH_BUFFER_MAGIC) {
		Print(L"fatal: eth buffer %lx (from %lx) bad magic %lx\n", buf, data, buf->magic);
		for (;;) ;
	}
	buf->next = eth_buffers;
	eth_buffers = buf;
}

int eth_send(void *data, size_t len) {
	EFI_STATUS r;

	if ((r = snp->Transmit(snp, 0, len, (void*) data, NULL, NULL, NULL))) {
		eth_put_buffer(data);
		return -1;
	} else {
		return 0;
	}
}

void eth_dump_status(void) {
	Print(L"State/HwAdSz/HdrSz/MaxSz %d %d %d %d\n",
		snp->Mode->State, snp->Mode->HwAddressSize,
		snp->Mode->MediaHeaderSize, snp->Mode->MaxPacketSize);
	Print(L"RcvMask/RcvCfg/MaxMcast/NumMcast %d %d %d %d\n",
		snp->Mode->ReceiveFilterMask, snp->Mode->ReceiveFilterSetting,
		snp->Mode->MaxMCastFilterCount, snp->Mode->MCastFilterCount);
	UINT8 *x = snp->Mode->CurrentAddress.Addr;
	Print(L"MacAddr %02x:%02x:%02x:%02x:%02x:%02x\n",
		x[0], x[1], x[2], x[3], x[4], x[5]);
	Print(L"SetMac/MultiTx/LinkDetect/Link %d %d %d %d\n",
		snp->Mode->MacAddressChangeable, snp->Mode->MultipleTxSupported,
		snp->Mode->MediaPresentSupported, snp->Mode->MediaPresent);
}

int eth_add_mcast_filter(const mac_addr *addr) {
	if (mcast_filter_count >= MAX_FILTER) return -1;
	if (mcast_filter_count >= snp->Mode->MaxMCastFilterCount) return -1;
	memcpy(mcast_filters + mcast_filter_count, addr, ETH_ADDR_LEN);
	mcast_filter_count++;
	return 0;
}

int netifc_open(EFI_HANDLE img, EFI_SYSTEM_TABLE *sys) {
	EFI_BOOT_SERVICES *bs = sys->BootServices;
	EFI_HANDLE h[32];
	EFI_STATUS r;
	UINTN sz;

	sz = sizeof(h);
	r = bs->LocateHandle(ByProtocol, &SimpleNetworkProtocol, NULL, &sz, h);
	if (r != EFI_SUCCESS) {
		Print(L"Failed to locate SNP handle(s) %x\n", r);
		return -1;
	}

	r = bs->OpenProtocol(h[0], &SimpleNetworkProtocol, (void**) &snp, img, NULL,
		EFI_OPEN_PROTOCOL_EXCLUSIVE);
	if (r) {
		Print(L"Failed to open SNP exclusively %x\n", r);
		return -1;
	}

	if (snp->Mode->State != EfiSimpleNetworkStarted) {
		snp->Start(snp);
		if (snp->Mode->State != EfiSimpleNetworkStarted) {
			Print(L"Failed to start SNP\n");
			return -1;
		}
		r = snp->Initialize(snp, 32768, 32768);
		if (r) {
			Print(L"Failed to initialize SNP\n");
			return -1;
		}
	}

	if (bs->AllocatePages(AllocateAnyPages, EfiLoaderData, NUM_BUFFER_PAGES, &eth_buffers_base)) {
		Print(L"Failed to allocate net buffers\n");
		return -1;
	}

	uint8_t *ptr = (void*) eth_buffers_base;
	for (r = 0; r < (NUM_BUFFER_PAGES * 2); r++) {
		eth_buffer *buf = (void*) ptr;
		buf->magic = ETH_BUFFER_MAGIC;
		eth_put_buffer(buf);
		ptr += 2048;
	}

	ip6_init(snp->Mode->CurrentAddress.Addr);

	r = snp->ReceiveFilters(snp,
		EFI_SIMPLE_NETWORK_RECEIVE_UNICAST |
		EFI_SIMPLE_NETWORK_RECEIVE_MULTICAST,
		0, 0, mcast_filter_count, (void*) mcast_filters);
	if (r) {
		Print(L"Failed to install multicast filters\n");
		return -1;
	}

	eth_dump_status();
	return 0;
}

void netifc_close(void) {
}

void netifc_poll(void) {
	UINT8 data[1514];
	EFI_STATUS r;
	UINTN hsz, bsz;
	UINT32 irq;
	VOID *txdone;

	if ((r = snp->GetStatus(snp, &irq, &txdone))) {
		return;
	}

	if (txdone) {
		eth_put_buffer(txdone);
	}
		
	if (irq & EFI_SIMPLE_NETWORK_RECEIVE_INTERRUPT) {
		hsz = 0;
		bsz = sizeof(data);
		r = snp->Receive(snp, &hsz, &bsz, data, NULL, NULL, NULL);
		if (r != EFI_SUCCESS) {
			return;
		}
#if TRACE
		Print(L"RX %02x:%02x:%02x:%02x:%02x:%02x < %02x:%02x:%02x:%02x:%02x:%02x %02x%02x %d\n",
			data[0], data[1], data[2], data[3], data[4], data[5],
			data[6], data[7], data[8], data[9], data[10], data[11],
			data[12], data[13], (int) (bsz - hsz));
#endif
		eth_recv(data, bsz);
	}
}

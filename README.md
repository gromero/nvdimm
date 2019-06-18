## Introduction

This tiny recipe shows how to get NVDIMM emulation on PPC64 using QEMU-kvm, i.e.
get a /dev/pmem0 device that can be mounted w/ `-o dax` flag and then be mapped
using new `MAP_SYNC` and `MAP_SHARED_VALIDATE` `mmap()` flags, so normal storage
load/store instructions can be used to read/write data to the persistent device.

It also presents a trivial test case (`ptest.c`) that allocates some space using
`mmap() + MAP_SYNC + MAP_SHARED_VALIDATE` and uses a store instruction `std` to
write one single character to the persistent device.

## QEMU-kvm

You need the following _out-of-tree_ patchset:

```
user@host:~/git/qemu$ git log --oneline -10
b5f2ad4937 (HEAD -> master) spapr: Add Hcalls to support PAPR NVDIMM device
ca4be1da89 spapr: Add NVDIMM device support
60d2bb021b mem: make nvdimm_device_list global
a7b21f6762 (origin/master, origin/HEAD) Merge remote-tracking branch 'remotes/vivier2/tags/linux-user-for-4.1-pull-request' into staging
<snip>
```

Then you can run the VM with pmem device emulated with the following command:
(don't forget to create a `.qcow2` disk first)
```
sudo ~/qemu/bin/qemu-system-ppc64 -nographic -display none -machine pseries,nvdimm=on,accel=kvm,usb=off,dump-guest-core=off -m 2G,slots=2,maxmem=4G -cpu host -cdrom /var/lib/libvirt/isos/ubuntu-19.04-server-ppc64el.iso -drive file=./image.qcow2,format=qcow2,if=none,id=drive-virtio-disk0 -device virtio-blk-pci,scsi=off,bus=pci.0,addr=0x3,drive=drive-virtio-disk0,id=virtio-disk0,bootindex=1 -net bridge,br=virbr0 -net nic,model=virtio -object memory-backend-file,id=memnvdimm0,prealloc=yes,mem-path=/home/user/nvdimm0,share=yes,size=1073872896 -device nvdimm,label-size=128k,uuid=75a3cdd7-6a2f-4791-8d15-fe0a920e8e9e,memdev=memnvdimm0,id=nvdimm0,slot=0 -smp 32,sockets=1,cores=4,threads=8
```

Important here are the `nvdimm` paramaters to enable and set NVDIMM device and
geometries and also ensure `-cpu host`, although it's supposed to be default.

That's all for QEMU-kvm.

## Linux

You'll need the following _out-of-tree_ patchset:
```
gromero@image:~/git/linux$ git log --oneline -10
8d7d63682145 (HEAD -> master_powerpc) mm: Move MAP_SYNC to asm-generic/mman-common.h
33e3792a9c41 mm/nvdimm: Fix endian conversion issues 
b43722793bfb mm/nvdimm: Use correct alignment when looking at first pfn from a region
0a891cc8b8a8 mm/nvdimm: Pick the right alignment default when creating dax devices
d4c6d65a5ab9 mm/nvdimm: Use correct #defines instead of opencoding
69cb511f838a mm/nvdimm: Add page size and struct page size to pfn superblock
ca9b3e77bf54 mm/nvdimm: Add PFN_MIN_VERSION support
45545ae522ec nvdimm: Consider probe return -EOPNOTSUPP as success
01ccc3ad4413 (source/master) Merge tag 'for-linus-20190610' of git://git.kernel.dk/linux-block
<snip>
```

Once pmem device is available, you will need to build the `ndctl` tool to set
them from the an specific branch `align-fix-4`:

```
user@guest:~/git/ndctl$ git branch -vv
* align-fix-4 878063a [origin/align-fix-4] libndctl, dimm: Don't require an xlat function
  master      521aa6e [origin/master] ndctl, test: convert remaining tests to use test/common
user@guest:~/git/ndctl$ git remote -vv
origin	https://github.com/oohal/ndctl.git (fetch)
origin	https://github.com/oohal/ndctl.git (push)
```

Then you can set the `/dev/pmem` device in the following way:
```
cd /home/user/git/ndctl/ndctl
sudo ./ndctl init-labels all -f
sudo ./ndctl enable-region region0
sudo ./ndctl create-namespace -e namespace0.0 -s 1G -f
sudo  mkfs.xfs -b size=64k -s size=512 /dev/pmem0 -f
sudo mount -o dax /dev/pmem0 /mnt/pmem/
```

Important thing here is to ensure `block size = 64k and section size = 512` when
formatting the `/dev/pmem0` device, otherwise you gonna see the following dmesg:
```
[  985.847837] XFS (pmem0): DAX unsupported by block device. Turning off DAX. <===
```

## Testing

Now you can use `ptest.c` to check if it's possible to write the pmem device
with DAX support. If you can, you must get a mmap like:
```
~/git/nvdimm$ ./ptest
Opening file /mnt/pmem/test/706 ...
File mmap'ed to address = 0x727733ad0000
```

You can use `ptest` also to verify what happens when pmem is not mounted with
'-o dax' flag, i.e. wo/ DAX support, or when you try to use it against any other
fs without DAX:
```
~/git/nvdimm$ ./ptest /tmp/blah
Opening file /tmp/blah/706 ...
mmap(): Operation not supported
```

## Thanks

Murilo Opsfelder
Shivaprasad Bhat
Vaibhav Jain
Oliver O'Halloran
Aneesh Kumar K.V

(All screw-ups are my fault)

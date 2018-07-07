#!/usr/bin/python2


import getopt
import os
import sys
import struct
import random
import time

DEVICE_FILES = {
    'rtc': 0,
    'mouse': 3,
    'taux': 4,
    'sound': 5,
    'tty': 6,
}


class FileInfo:

    def __init__(self, dir_name, file_name, inode):
        self.file_name = file_name
        self.file_name_32B = file_name[0:32]

        if len(self.file_name_32B) < 32:
            self.file_name_32B += ('\x00' * (32 - len(self.file_name_32B)))

        file_type = DEVICE_FILES.get(file_name)
        if file_type is not None:
            self.file_type = file_type
        elif file_name == '.':
            self.file_type = 1
        else:
            self.file_type = 2

        if file_name in ['.'] + DEVICE_FILES.keys():
            self.file_size = 0
        else:
            self.file_size = os.path.getsize(os.path.join(dir_name, file_name))

        if self.file_size % 4096 != 0:
            self.data_block_num = self.file_size / 4096 + 1
        else:
            self.data_block_num = self.file_size / 4096

        self.inode = inode

        self.data_blocks = None

        return

    def set_data_blocks(self, data_blocks):
        self.data_blocks = data_blocks
        return


def _usage():
    print 'Usage:'
    print '  ' + sys.argv[0] + ' [options]\n'
    print 'Options:'
    print '  -h, --help                 Show help.'
    print '  -i, --input <path>         Path to input directory.'
    print '  -o, --output <path>        Path to output file.\n'
    return


def _main():
    random.seed()

    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hi:o:', ['--help', 'input=', 'output='])
    except getopt.GetoptError:
        print 'error: invalid options\n'
        _usage()
        sys.exit(1)

    arg_input = None
    arg_output = None
    for o, a in opts:
        if o in ('-i', '--input'):
            arg_input = a
        elif o in ('-o', '--output'):
            arg_output = a
        elif o in ('-h', '--help'):
            _usage()
            sys.exit(0)
        else:
            print 'error: invalid options\n'
            _usage()
            sys.exit(1)

    if arg_input is None or arg_output is None:
        print 'error: missing options\n'
        _usage()
        sys.exit(1)

    if not os.path.isdir(arg_input):
        print 'error: input is not a directory\n'
        sys.exit(1)

    if os.path.isfile(arg_output):
        os.remove(arg_output)

    for f in DEVICE_FILES:
        if not os.path.isfile(os.path.join(arg_input, f)):
            open(os.path.join(arg_input, f), 'w').close()

    f = open(os.path.join(arg_input, 'created.txt'), 'w')
    f.write('%s\n' % (time.strftime('%Y-%m-%d, %H:%M:%S', time.localtime())))
    f.close()

    fs_file_names = ['.']
    fs_file_names.extend([
        f for f in os.listdir(arg_input)
        if os.path.isfile(os.path.join(arg_input, f))
        and not f == '.gitignore'
    ])

    fs_dentry_num = len(fs_file_names)
    if fs_dentry_num > 63:
        print 'error: too many files, max is 63\n'
        sys.exit(1)

    # create some unused inodes, since the max inode in use will be the
    # same as max dentry num, we just add 1 more to that so 64 inode blocks
    # and inode 0 should be used as a NULL block
    fs_inode_num = 64

    # create a list of inode id's to choose from
    fs_inode_list = [i for i in xrange(fs_inode_num)]
    fs_inode_dict = dict.fromkeys(fs_inode_list, None)

    # after creating the inode dictionary to map file names
    # we need to remove inode 0 from the list since we are
    # using random.choice() to pick random inode to assign
    fs_inode_list.remove(0)

    # list for checking duplicate 32 byte file names
    file_name_32B_check = []

    fs_files = {}
    fs_data_block_num = 0

    # initialize file information needed
    for file_name in fs_file_names:

        # directory and device files don't have an inode
        if file_name in ['.'] + DEVICE_FILES.keys():
            inode = 0
        else:
            inode = random.choice(fs_inode_list)
            fs_inode_list.remove(inode)
            fs_inode_dict[inode] = file_name

        fs_files[file_name] = FileInfo(arg_input, file_name, inode)
        fs_data_block_num += fs_files[file_name].data_block_num
        if fs_files[file_name].file_name_32B not in file_name_32B_check:
            file_name_32B_check.append(fs_files[file_name].file_name_32B)
        else:
            print 'error: duplicate 32 byte file name "' + fs_files[file_name].file_name_32B + '"\n'
            sys.exit(1)

    # add some extra 25 unused data blocks in the system (extra 100KB)
    # and data block 0 should be used as a NULL block
    fs_data_block_num += 25

    # create a list of data block id's to choose from
    fs_data_block_list = [i for i in xrange(fs_data_block_num)]
    fs_data_block_dict = dict.fromkeys(fs_data_block_list, None)

    # after creating the data block dictionary to map file names
    # we need to remove data block 0 from the list since we are
    # using random.sample() to pick random data blocks to assign
    fs_data_block_list.remove(0)

    # assign data blocks to files
    for key in fs_files:
        data_blocks = random.sample(fs_data_block_list, fs_files[key].data_block_num)
        for each in data_blocks:
            fs_data_block_dict[each] = fs_files[key].file_name

        if len(data_blocks):
            fs_data_block_list = [x for x in fs_data_block_list if x not in data_blocks]

        fs_files[key].set_data_blocks(data_blocks)

    padding_4KB = '\x00' * 4096
    padding_64B = '\x00' * 64
    padding_52B = '\x00' * 52
    padding_24B = '\x00' * 24

    # create the boot block
    boot_block = ''
    inode_blocks = ''
    data_blocks = ''

    boot_block += struct.pack('<I', fs_dentry_num)
    boot_block += struct.pack('<I', fs_inode_num)
    boot_block += struct.pack('<I', fs_data_block_num)
    boot_block += padding_52B

    # we have at most 63 dentries
    dentry_count = 63

    # make sure the first dentry is the "."
    boot_block += fs_files['.'].file_name_32B
    boot_block += struct.pack('<I', fs_files['.'].file_type)
    boot_block += struct.pack('<I', fs_files['.'].inode)
    boot_block += padding_24B
    dentry_count -= 1

    # fill up the rest of the boot block with other file information
    for key in fs_files:
        if key != '.':
            boot_block += fs_files[key].file_name_32B
            boot_block += struct.pack('<I', fs_files[key].file_type)
            boot_block += struct.pack('<I', fs_files[key].inode)
            boot_block += padding_24B
            dentry_count -= 1

    # pad the boot block
    for i in xrange(dentry_count):
        boot_block += padding_64B

    print 'size of boot block (in bytes):', len(boot_block)

    # go through all inodes in the list, if this inode is assigned
    # to a file, then get the information and write it to inode blocks
    for i in xrange(fs_inode_num):
        file_name = fs_inode_dict[i]
        if file_name is None:
            inode_blocks += padding_4KB
        else:
            inode_size = 4096

            inode_blocks += struct.pack('<I', fs_files[file_name].file_size)
            inode_size -= 4

            for each in fs_files[file_name].data_blocks:
                inode_blocks += struct.pack('<I', each)
                inode_size -= 4

            # pad the current inode block if necessary
            inode_blocks += '\x00' * inode_size

    print 'size of inode blocks (in bytes):', len(inode_blocks)

    # same idea as the inode, go through all data blocks and check if it
    # has been assigned to a file, if it has been assigned then open
    # the file, get the data needed, and write it to the data blocks
    for i in xrange(fs_data_block_num):
        file_name = fs_data_block_dict[i]
        if file_name is None:
            data_blocks += padding_4KB
        else:
            data_block_size = 4096

            # the trick here is getting the data section we need in the right order
            # since the file information has data block index list, we check the
            # current data block index and see the position of it in the pre-mentioned
            # list, then we seek the file, skip over the data we don't need and read
            # 4KB data. don't forget to check if there is actually 4kb read, if not
            # we need to pad the data block.
            in_file = open(os.path.join(arg_input, file_name), 'r')
            data_block_idx = fs_files[file_name].data_blocks.index(i)
            in_file.seek(data_block_idx * 4096, 0)
            data = in_file.read(4096)
            in_file.close()

            data_size = len(data)
            data_block_size -= data_size
            data_blocks += data

            data_blocks += '\x00' * data_block_size

    print 'size of data blocks (in bytes):', len(data_blocks)

    out_file = open(arg_output, 'w')
    out_file.write(boot_block)
    out_file.write(inode_blocks)
    out_file.write(data_blocks)
    out_file.close()

    for f in ['created.txt'] + DEVICE_FILES.keys():
        try:
            os.remove(os.path.join(arg_input, f))
        except IOError:
            pass
    sys.exit(0)


if __name__ == '__main__':
    _main()

#############################################################################
#                       WARNING
#  If different registers (other than r11 and r10) are chosen,
#  the pattern should be modified.
#
#  We only use this tool to show it is possible to rewrite the exe and shared libraries.
#  Currently it works and we can run Firefox-79.0 successfully after binary rewriting.
#  Both the prologue and epilogue are quite unique.
#  But more sanity checks might be needed to make sure only the code sections are modified.
#
#  TBD: both big-endian and little endian should be considered. We assume little-endian.
#
#  Usage:
#
#       python3 code_rewriter.py ./ 8192 4096
#
#       the stack size of an exe/so is 8192;  that of the output is 4096
#
#
#
#                                                    sheisc@163.com
##############################################################################

import os
import sys


# 0000000000400b10 <main>:
#   400b10:	49 89 e3             	mov    %rsp,%r11
#   400b13:	49 81 e3 00 00 80 ff 	and    $0xffffffffff800000,%r11
#   400b1a:	4d 8b 9b 00 00 00 ff 	mov    -0x1000000(%r11),%r11
#   400b21:	4c 03 1c 24          	add    (%rsp),%r11
#   400b25:	4c 89 9c 24 00 00 80 	mov    %r11,-0x800000(%rsp)
#   400b2c:	ff
#
#   ......
#
#   400bd0:	49 89 e2             	mov    %rsp,%r10
#   400bd3:	49 81 e2 00 00 80 ff 	and    $0xffffffffff800000,%r10
#   400bda:	4c 8b 9c 24 00 00 80 	mov    -0x800000(%rsp),%r11
#   400be1:	ff
#   400be2:	4d 2b 9a 00 00 00 ff 	sub    -0x1000000(%r10),%r11
#   400be9:	48 83 c4 08          	add    $0x8,%rsp
#   400bed:	41 ff e3             	jmpq   *%r11


# instrumented code at the function entry
prolog_pattern = b"\x49\x89\xe3" \
                 b"\x49\x81\xe3\x00\x00\x80\xff" \
                 b"\x4d\x8b\x9b\x00\x00\x00\xff" \
                 b"\x4c\x03\x1c\x24" \
                 b"\x4c\x89\x9c\x24\x00\x00\x80\xff"

# the indexes of the three magic numbers in prolog_pattern needed to be rewritten
# \x00\x00\x80\xff          -8M in 32-bit, little-endian
# \x00\x00\x00\xff          -16M in 32-bit
# \x00\x00\x80\xff          -8M in 32-bit
prolog_indexes = (6, 13, 25)

# instrumented code at the function exit
epilog_pattern = b"\x49\x89\xe2" \
                 b"\x49\x81\xe2\x00\x00\x80\xff" \
                 b"\x4c\x8b\x9c\x24\x00\x00\x80\xff" \
                 b"\x4d\x2b\x9a\x00\x00\x00\xff" \
                 b"\x48\x83\xc4\x08" \
                 b"\x41\xff\xe3"

# the indexes of the three magic numbers in epilog_pattern needed to be rewritten
# \x00\x00\x80\xff          -8M in 32-bit, little-endian
# \x00\x00\x80\xff          -8M in 32-bit
# \x00\x00\x00\xff          -16M in 32-bit
epilog_indexes = (6, 14, 21)

#####################################################################################

def print_bytes(bs):
    print(''.join('\\x{0:02x}'.format(b) for b in bs))


# get the low four bytes of an integer
def get_four_bytes(x):
    bs = []
    i = 0
    while i < 4:
        b = (x & 0xFF)
        bs.append(b)
        x >>= 8
        i += 1
    return bs


#  @b_arr,  a byte array to be modified
#  @i,  the start position
#  @bs, the bytes used to modify the b_arr
#
def modify_four_bytes(b_arr, i, bs):
    n = len(bs)
    k = 0
    while k < n:
        b_arr[i+k] = bs[k]
        k += 1
    return b_arr


#
# @b_arr is the byte array of a binary fie,
#
# @i is the position of a function prolog we find,
#
# @x is the size of call stack,
#
#    the low 4 bytes of (-x) will be written into the binary
#
def modify_prolog_bytes(b_arr, i, x):
    bs = get_four_bytes(-x)
    modify_four_bytes(b_arr, prolog_indexes[0] + i, bs)
    modify_four_bytes(b_arr, prolog_indexes[2] + i, bs)
    bs = get_four_bytes(-2 * x)
    modify_four_bytes(b_arr, prolog_indexes[1] + i, bs)
    return b_arr


#
# @b_arr is the byte array of a binary fie,
#
# @i is the position of a function epilog we find,
#
# @x is the size of call stack,
#
#    the low 4 bytes of (-x) and (-2 * x) will be written into the binary
#
def modify_epilog_bytes(b_arr, i, x):
    bs = get_four_bytes(-x)
    modify_four_bytes(b_arr, epilog_indexes[0] + i, bs)
    modify_four_bytes(b_arr, epilog_indexes[1] + i, bs)
    bs = get_four_bytes(-2 * x)
    modify_four_bytes(b_arr, epilog_indexes[2] + i, bs)
    return b_arr


# compare two byte arrays
def is_matching(bin_img, i, pattern):
    n = len(bin_img)
    m = len(pattern)
    k = 0;
    while k < m and (k + i) < n:
        if bin_img[k + i] != pattern[k]:
            return False
        k += 1
    return k == m


# modify all prologs and epilogs in bin_img, such that the call stack size is @x
def modify_all_prologs_and_epilogs(bin_img, prolog, epilog, x, file_len):
    i = 0
    n = len(bin_img)
    mp = len(prolog)
    me = len(epilog)
    m = min(me, mp)
    pro_cnt = 0
    epi_cnt = 0
    percent = 0
    while i <= n - m:  #
        # how much progress we have made
        if (i * 100 // file_len) > percent:
            print(">", end=" ", flush=True)
            percent += 5

        if is_matching(bin_img, i, prolog):
            modify_prolog_bytes(bin_img, i, x)
            pro_cnt += 1
            i += mp
        elif is_matching(bin_img, i, epilog):
            modify_epilog_bytes(bin_img, i, x)
            epi_cnt += 1
            i += me
        else:
            i += 1
    return pro_cnt, epi_cnt


# exe and *.so
def modify_one_bin_file(input_bin_path, output_bin_path:str, orig_x, new_x):
    # in KB, to be consistent with 'ulimit -s'
    orig_x *= 1024
    new_x *= 1024

    prolog_arr = bytearray(prolog_pattern)
    epilog_arr = bytearray(epilog_pattern)
    # create prolog and epilog patterns for x
    modify_prolog_bytes(prolog_arr, 0, orig_x)
    modify_epilog_bytes(epilog_arr, 0, orig_x)

    print(input_bin_path, end=" ", flush=True)

    with open(input_bin_path, "rb") as f:
        bin_img = bytearray(f.read())
        pro_cnt, epi_cnt = modify_all_prologs_and_epilogs(
                                bin_img, prolog_arr, epilog_arr, new_x, len(bin_img))

    with open(output_bin_path, "wb") as f:
        f.write(bin_img)

    print(output_bin_path, pro_cnt, epi_cnt)

    if not output_bin_path.endswith(".so") and pro_cnt > 0 and epi_cnt > 0:
        os.system('chmod a+x ' + output_bin_path)

    # print("orig_x = 0x{0:x}, new_x = 0x{1:x}, \ninput = {2}, \noutput = {3}".format(
    #     orig_x, new_x, input_bin_path, output_bin_path))

    # check the stack size is large enough
    stack_size = int(os.popen("ulimit -s").read())
    if stack_size * 1024 < new_x * 4:
        print("Please reset the stack size (>= {0} KB): ulimit -s {0}".format(4 * new_x//1024))
    # else:
    #     print("'ulimit -s' is {0} KB".format(stack_size))

    # TBD: sanity-checking for the code sections


# ELF executable or *.so,  but excluding *.o,
def is_elf_file(fpath):
    with open(fpath, "rb") as f:
        img = bytearray(f.read())
        bs = b"\x7FELF"
        return is_matching(img, 0, bs) and not fpath.endswith(".o")


def main():
    target_path = sys.argv[1]
    orig_x = int(sys.argv[2])
    new_x = int(sys.argv[3])

    g = os.walk(target_path)

    for path, dir_list, file_list in g:
        # os.walk() seems to be depth-first traverse
        # print("\n\n" + path + "\n\n")
        for f in file_list:
            fpath = os.path.join(path, f)
            if is_elf_file(fpath):
                modify_one_bin_file(fpath, fpath, orig_x, new_x)


if __name__ == '__main__':
    main()






    
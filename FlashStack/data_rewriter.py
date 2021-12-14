#############################################################################
#                       NOTING
#
#  For binary-rewriting the global variable about stack size in the main executable. 
#  TBD: both big-endian and little endian should be considered. We assume little-endian.
#
#  Usage:
#
#       python3 data_rewriter.py ./ 8192 4096
#
#       the stack size of an exe is 8192;  that of the output is 4096
#
#
#
#                                                    sheisc@163.com
##############################################################################

import os
import sys




# 4K pape in the main executable for storing the global stack size
global_stack_size_pattern = b"\x00\x00\x80\xff\xff\xff\xff\xff" \
                            b"CSE@UNSWCSE@UNSWCSE@UNSW" +  b"\x00" * (4096 - 32)



#####################################################################################

def print_bytes(bs):
    print(''.join('\\x{0:02x}'.format(b) for b in bs))


# get the low n bytes of an integer
def get_n_bytes(x, n):
    bs = []
    i = 0
    while i < n:
        b = (x & 0xFF)
        bs.append(b)
        x >>= 8
        i += 1
    return bs


#  @b_arr,  a byte array to be modified
#  @i,  the start position
#  @bs, the bytes used to modify the b_arr
#
def modify_bytes(b_arr, i, bs):
    n = len(bs)
    k = 0
    while k < n:
        b_arr[i+k] = bs[k]
        k += 1
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
def modify_global_stack_size(bin_img, pattern, new_x, file_len):
    i = 0
    n = len(bin_img)
    m = len(pattern)
    bs = get_n_bytes(-new_x, 8)

    cnt = 0
    percent = 0
    while i <= n - m:  #
        if (i * 100 // file_len) > percent:
            print(">", end=" ", flush=True)
            percent += 5
        if is_matching(bin_img, i, pattern):
            modify_bytes(bin_img, i, bs)
            cnt += 1
            i += m
        else:
            i += 1
    return cnt


# exe and *.so
def modify_one_bin_file(input_bin_path, output_bin_path, orig_x, new_x):
    # in KB, to be consistent with 'ulimit -s'
    orig_x *= 1024
    new_x *= 1024

    pattern_arr = bytearray(global_stack_size_pattern)

    # create prolog and epilog patterns for x
    bs = get_n_bytes(-orig_x, 8)
    modify_bytes(pattern_arr, 0, bs)

    print(input_bin_path, end=" ", flush=True)

    with open(input_bin_path, "rb") as f:
        bin_img = bytearray(f.read())
        cnt = modify_global_stack_size(
                                bin_img, pattern_arr, new_x, len(bin_img))

    # if cnt > 1:
    #     print("Something WRONG. OOPS, so sad, the executable is alien to us ...") 
    #     return   

    with open(output_bin_path, "wb") as f:
        f.write(bin_img)

    print(output_bin_path, cnt)

    if not output_bin_path.endswith(".so") and cnt == 1:
        os.system('chmod a+x ' + output_bin_path)


    # check the stack size is large enough
    stack_size = int(os.popen("ulimit -s").read())
    if stack_size * 1024 < new_x * 4:
        print("Please reset the stack size (>= {0} KB): ulimit -s {0}".format(4 * new_x//1024))



# ELF 
def is_elf_file(fpath):
    with open(fpath, "rb") as f:
        img = bytearray(f.read())
        bs = b"\x7FELF"
        return is_matching(img, 0, bs) and not fpath.endswith(".o") #and not fpath.endswith(".so")


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






    
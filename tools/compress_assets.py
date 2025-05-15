#!/usr/bin/env python

import gzip
import os
from argparse import ArgumentParser
from collections import OrderedDict
from fnmatch import fnmatch

def compress_file(file_path, outdir = '.'):
    """
    Compress a file using gzip and print the compression stats.
    """
    # Read the file
    with open(file_path, 'rb') as f:
        data = f.read()

    initial_len = len(data)

    # Compress the file
    gzip_level = 9
    data = gzip.compress(data, gzip_level)
    data_len = len(data)

    if initial_len < 1024:
        initial_len_str = '%d B' % (initial_len)
        data_len_str = '%d B' % (data_len)
    elif initial_len < 1024 * 1024:
        initial_len_str = '%.1f KiB' % (initial_len / 1024)
        data_len_str = '%.1f KiB' % (data_len / 1024)
    else:
        initial_len_str = '%.1f MiB' % (initial_len / 1024 / 1024)
        data_len_str = '%.1f MiB' % (data_len / 1024 / 1024)

    percent = 100.0
    if initial_len > 0:
        percent = data_len / initial_len * 100.0

    stats = '%-9s -> %-9s (%.1f%%)' % (initial_len_str, data_len_str, percent)
    print('%-34s file %s' % (file_path, stats))

    # Write the compressed data to a new file in the target directory
    if not os.path.exists(outdir):
        os.makedirs(outdir)
    file_name = os.path.basename(file_path)
    file_path = os.path.join(outdir, file_name)
    if os.path.exists(file_path + '.gz'):
        os.remove(file_path + '.gz')
    # Write the compressed data to a new file
    # in the target directory
    with open(file_path + '.gz', 'wb') as f:
        f.write(data)



if __name__ == '__main__':
    parser = ArgumentParser(description='Compress assets using gzip')
    parser.add_argument('files', nargs='+', help='File(s) to compress')
    parser.add_argument('-o', '--outdir', default='.', help='Directory to save compressed files')
    parser.add_argument('-e', '--exclude', action='append', help='Exclude files matching the pattern')
    args = parser.parse_args()

    # Exclude files matching the patterns
    if args.exclude:
        exclude_patterns = args.exclude
        args.files = [f for f in args.files if not any(fnmatch(f, pattern) for pattern in exclude_patterns)]

    # Compress each file
    for file_path in args.files:
        compress_file(file_path, args.outdir)

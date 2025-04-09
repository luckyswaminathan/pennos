Test mounting and unmounting
  $ $TESTDIR/../../../bin/pennfat << EOF
  > mkfs testfs 4 4
  > mount testfs
  > unmount
  > EOF
  > hexdump testfs
  PENNFAT> PENNFAT> PENNFAT> PENNFAT> 
  0000000 0404 ffff 0000 0000 0000 0000 0000 0000
  0000010 0000 0000 0000 0000 0000 0000 0000 0000
  *
  2003000

Test touching a file to create it
  $ $TESTDIR/../../../bin/pennfat << EOF
  > mount testfs
  > touch aaaa
  > unmount
  > EOF
  > hexdump testfs
  PENNFAT> PENNFAT> PENNFAT> PENNFAT> 
  0000000 0404 ffff ffff 0000 0000 0000 0000 0000
  0000010 0000 0000 0000 0000 0000 0000 0000 0000
  *
  0004000 6161 6161 0000 0000 0000 0000 0000 0000
  0004010 0000 0000 0000 0000 0000 0000 0000 0000
  0004020 0000 0000 0002 0600 1670 67f6 0000 0000
  0004030 0000 0000 0000 0000 0000 0000 0000 0000
  *
  2003000

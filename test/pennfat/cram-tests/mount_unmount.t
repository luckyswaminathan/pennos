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

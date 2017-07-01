# File System

## Operating Systems Programming Project with xv6





### 1. Implementation Specification

- #### Expand the maximum size of a file.

  The goal of this project is to expand the maximum size a file can have. As you go
  through the project, you will learn more about how xv6 is building the its file system.
  The structure of the inode that xv6 currently has is shown below. Refer the figure and
  Implement a double indirect block to increase the capacity of the file.

  ​

### 2. Design and Implementation

#### 1. Change *dinode* & *inode* structure

- Current inode contains data block of its file by 12 direct pointer and 1 indirect pointer. In xv6, each size of block is 512Byte, so a file can contain 6,144Byte with 12 direct pointer(12 * 512Byte), and 65,536Byte with 1 indirect pointer(128 * 512Byte). Thus, a file can represent up to 70KB size. Without changing inode structure size to increase maximum size of file, replace one direct pointer to double indirect pointer. Thereby, increase the maximum size of a file to 8MB.


-  현재 inode는 file의 metadata로 data block을 12개의 direct pointer와 1개의 indirect pointer로 나타내고 있다. xv6에서 각 block의 size는 512Byte이므로, 한 file은 12개의 direct pointer로 12 * 512Byte = 6,144Byte를, 1개의 indirect pointer로 128 * 512Byte = 65,536Byte를 나타낼 수 있다. 따라서, 한 개의 file은 최대 70KB를 나타낼 수 있어, test program의 10MB를 정상적으로 write할 수 없다.  inode의 크기는 바꾸지 않으면서 file의 최대 size를 늘이기 위해서, direct pointer 1개를 double indirect pointer로 바꾸어 한 file의 최대 size를 약 8MB까지 늘인다.

  ```c
  #define NDIRECT 11
  #define NINDIRECT (BSIZE / sizeof(uint))
  #define N2INDIRECT (BSIZE / sizeof(uint)) * (BSIZE / sizeof(uint))
  #define MAXFILE (NDIRECT + NINDIRECT + N2INDIRECT)

  struct dinode {
    short type;           // File type
    short major;          // Major device number (T_DEV only)
    short minor;          // Minor device number (T_DEV only)
    short nlink;          // Number of links to inode in file system
    uint size;            // Size of file (bytes)
    uint addrs[NDIRECT+2];   // Data block addresses
  };
  ```



#### 2. Change *bmap()* function

- Using *bmap(struct inode *ip, uint bn)*, you can get disk block number of bn-th data block of inode. Unlike before, the data block size of inode has been incresed by additional double indirect size, bmap need to change to return disk data blocknumber of that double indirect block.

- bmap으로 inode의 bn번째 data block의 disk block number를 가져온다. 이전과는 달리, inode가 가질 수 있는 data block의 크기가 double indirect만큼 늘어났으므로, 그만큼 더 data block number를 return 해 줄 수 있도록 한다.

  ```c
  bn -= NINDIRECT;
  if(bn < N2INDIRECT){
    // Load double indirect block, allocating if necessary.
    if((addr = ip->addrs[NDIRECT+1]) == 0)
      ip->addrs[NDIRECT+1] = addr = balloc(ip->dev);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn/(BSIZE / sizeof(uint))]) == 0){
      a[bn/(BSIZE / sizeof(uint))] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    bp = bread(ip->dev, addr);
    a = (uint*)bp->data;
    if((addr = a[bn % (BSIZE / sizeof(uint))]) == 0){
      a[bn % (BSIZE / sizeof(uint))] = addr = balloc(ip->dev);
      log_write(bp);
    }
    brelse(bp);
    return addr;
  }
  ```



#### 3. Change *itrunc()* function

- Using *itrunc(struct inode *ip)*, free all data block of the inode to remove the inode. Unlike before, data block of inode has been increased by additional double indirect, itrunc need to change its code to remove increased data block.

- itrunc 함수로 해당 inode의 모든 data block을 free 해주어 inode를 없앤다. 이전과는 달리, data block이 double indirect 만큼 늘어났으므로, 늘어난 data block 역시 free 해주도록 코드를 추가해준다. 

  ```c
  if(ip->addrs[NDIRECT+1]){
    midbp = bread(ip->dev, ip->addrs[NDIRECT+1]);
    a = (uint*)midbp->data;
    for(k = 0; k < NINDIRECT; k++){
      if(a[k]){
        bp = bread(ip->dev, a[k]);
        b = (uint*)bp->data;
        for(l = 0; l < NINDIRECT; l++){
          if(b[l])
            bfree(ip->dev, b[l]);
        }
        brelse(bp);
        bfree(ip->dev, a[k]);
      }      
    }
    brelse(midbp);
    bfree(ip->dev, ip->addrs[NDIRECT+1]);
    ip->addrs[NDIRECT+1] = 0;
  }
  ```
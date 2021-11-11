## 编译环境
32位Linux系统

## 运行环境

bochs虚拟机

## 编译&运行

### 下载代码&编译

```
git clone git@github.com:jerrykcode/os.git

cd os

make mbr.bin    # 编译MBR
make loader.bin # 编译内核加载器
make kernel.bin # 编译内核
```


### 安装bochs虚拟机&配置bochsrc.disk文件

可参考《操作系统真象还原》第46页

1. 下载bochs  

    在 https://sourceforge.net/projects/bochs/files/bochs/ 选择下载bochs

2. 解压压缩包   

    进入下载目录，  
```
tar zxvf bochs-x.x.x.tar.gz
```

3. 编译bochs  

    引用《操作系统真象还原》第47页  

    先进入到目录 cd bochs-x.x.x，开始 configure、make、make install 三步曲  
```
./configure \  
--prefix=/your_path/bochs \  
--enable-debugger \  
--enable-disasm \  
--enable-iodebug \  
--enable-x86-debugger \  
--with-x \  
--with-x11  
```

    注意各行结尾的'\'字符前面有个空格。下面简要说明一下 configure 的参数。

    --prefix=/your_path/bochs 是用来指定 bochs 的安装目录，根据个人实际情况将 your_path 替换为自己待安装的路径。  
    --enable-debugger 打开 bochs 自己的调试器。  
    --enable-disasm 使 bochs 支持反汇编。  
    --enable-iodebug 启用 io 接口调试器。  
    --enable-x86-debugger 支持 x86 调试器。  
    --with-x 使用 x windows。  
    --with-x11 使用 x11 图形用户接口。  

    configure 之后，会生成 Makefile，可以开始编译了。  
```
make
```
    若编译时没有问题，就直接执行下面这句。  
```
make install
```

4. 配置bochs

    修改 bochsrc.disk文件  

    将第3行romimage, 第4行vgaromimage, 以及第11行keyboard后面的bochs路径"/home/jerry/bochs"修改为自己的  

### 制作虚拟硬盘

    这一步要创建一个60M主盘(内核安装在主盘)，以及一个80M从盘(文件系统安装在从盘)  

1. 创建名为hd60M.img的60M硬盘.  

    在bochs安装目录下，执行  
```
bin/bximage -mode=create -hd=60 -q hd60M.img
```
    再次修改bochsrc.disk文件，将第17行hd60M.img的路径改为自己的  

2. 创建名为hd80M.img的从盘  

    在bochs安装目录下，执行  
```
bin/bximage -mode=create -hd=80 -q hd80M.img
```
    再次修改bochsrc.disk文件，将第18行hd80M.img的路径改为自己的  

3. 为从盘分区  

    在bochs安装目录下，  

    使用fdisk对hd80M.img进行分区，创建一个主分区以及一个扩展分区，  
    再创建若干个逻辑分区(扩展分区分多个逻辑分区)，  
    之后使用 t 命令将逻辑分区的Type改为0x66(unknown)  

### 运行OS

    回到代码目录, 执行run脚本  
```
sh run.sh
```

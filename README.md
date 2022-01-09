#迷你型类Linux操作系统

此项目的灵感来源于 MIT 6.828 的课程
由于本人水平有限,又由于只想写一个玩具操作系统，所以部分代码可能不是很规范，注释也是很少，请大家谅解
此操作系统在重定向和管道功能上有点小bug，由于时间原因我就不debug了


开发环境:

  Ubuntu14.04+
  GCC version 5.2.1

测试环境:

  QEMU emulator version 2.3.0
  ThinkPad E431

具体实现与特点:

  1、可在真机上运行

  2、实现buddy分页系统，slub内存分配系统

  3、实现VFS(虚拟文件系统)

  4、实现各种库函数(eg.  malloc)

  5、支持SMP，也就是多处理器


使用方法:

  下载源码后,在终端输入make qemu,会自动完成内核的编译，然后内核就会运行在qemu虚拟机里

制作u盘启动:
一会再写。。

##<a name = "index"/>目录
* [操作系统介绍](#背景介绍)
* [加载器介绍介绍](#jack语言介绍) 
* [ELF文件格式介绍](#语法要素)
* [Buddy系统介绍](#语法要素)
* [slub内存分配系统介绍](#语法要素)


















A mini operating system.

You need bochs or qemu to boot this os.

type make on the terminal to compile the kernel
and type make boch or make qemu to start kernel up.

You need sudo apt-get install xorriso.

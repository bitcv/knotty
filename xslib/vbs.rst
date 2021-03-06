****************
  VBS 编码方法 
****************
:Author: 熊家贵
:Id: $Id: vbs.rst,v 1.9 2015/05/28 03:21:34 gremlin Exp $


VBS编码是计算机编程语言中最常见数据类型及数据结构的一种计算机外部表示方法。
计算机编程语言中的数据编码成VBS格式后，可以方便的进行网络传输及持久存储。


支持数据类型
==============

VBS编码支持以下数据类型和数据结构 ::

        integer         %i 整数
        floating        %f 二进制浮点数(编程语言中的float, double等)
        decimal         %d 十进制浮点数(目前多数编程语言没有原生支持)
        bool            %t 布尔
        string          %s 字符串
        blob            %b 二进制数据
        list            [] 列表(数组) %l
        dict            {} 字典(键值对的集合) %m
        null            %n 空类型

除此之外，VBS编码还允许在这些数据类型前附加一个描述符(descriptor)。该描述符
的含义和用处，下文会进行详细的说明。

VBS编码将数据编码成二进制的形式，并且将数据的类型也编码到编码后的字节流中，
因此和一般的二进制编码不同，VBS不需要额外的信息就能进行正确的解码。 从逻辑
上看，VBS编码和JSON很像(几乎是一样的)，只是JSON是文本的，VBS是二进制的。

VBS并不规定字符串的字符集，这由应用自己决定。


编码详细描述
==============

计算机语言中的数据通常分为基本类型(整数，浮点数，布尔值，字符串等)和复合类型
(列表，字典等)。


整数
---------

整数在VBS中被编码成一个或多个字节，整数的值(绝对值)越小，编码后的字节数越少。

整数编码后的多个字节，除了最后一个字节外，每个字节的最高位都置为1，剩下的7个
比特用来保存整数的二进制补码表示的一组7比特数据。最后一个字节的最高3比特为
``010`` (正整数) 或 ``011`` (负整数), 剩下的5比特用来保存整数的二进制补码
表示的5比特数据。编码时，整数的二进制补码的低位在前，高位在后。如下图所示::

        LSB                                 MSB
        +---------+---------+.......+---------+
        |1xxx xxxx|1xxx xxxx|       |010x xxxx| 正整数
        +---------+---------+.......+---------+

        +---------+---------+.......+---------+
        |1xxx xxxx|1xxx xxxx|       |011x xxxx| 负整数
        +---------+---------+.......+---------+

比如，整数 1 编码成::

        +---------+
        |0100 0001|
        +---------+

整数 -32 编码成::

        +---------+---------+
        |1010 0000|0110 0000|
        +---------+---------+


浮点数
------------

浮点数可以表示成 ``sign * fraction * 2 ^ exponent`` 的形式, 其中 sign 表示正负号，
fraction 表示浮点数的有效数字(整数), exponent 表示2的多少次幂(整数)。
按照上面整数编码的思想，浮点数编码如下图所示::

        +---------+---------+.......+---------+
        |1fff ffff|1fff ffff|       |0001 111s| 0x1E + s
        +---------+---------+.......+---------+
        | exponent as integer
        +-----------------------------

其中 ``fff`` 为 fraction 的二进制补码按照低位前高位后，7比特一组的方式保存的
数据, s 表示正或负(1为负), exponent 按照普通的整数进行编码。

浮点数中的几个特殊值用特殊的方式来表示::

        value   sign    frac    expo
        +0.0    0       0       +1
        -0.0    0       0       -1
        +INF    0       0       +2
        -INF    0       0       -2
         NAN    0       0       >=+3
         NAN    0       0       <=-3


布尔值
-----------

布尔值编码成一个字节。如下图所示，其中x为1表示真，0表示假。 ::

        +---------+  
        |0001 100x|                     0x18 + x
        +---------+


字符串
-----------

字符串编码成字符串长度+字符串数据。字符串长度和数据不包括C语言中的结尾的
nil字符。字符串长度编码如下图所示::

        +---------+---------+.......+---------+
        |1xxx xxxx|1xxx xxxx|       |001x xxxx|
        +---------+---------+.......+---------+


blob二进制数据
-------------------

blob码成长度+数据。blob长度编码如下图所示::

        +---------+---------+.......+---------+
        |1xxx xxxx|1xxx xxxx|       |0001 1011| 0x1B
        +---------+---------+.......+---------+


list类型
-------------

list类型数据编码成 (可选的长度) + list开头字节 + list所有元素 + 结尾字节。如下图所示::

        +---------------------
        | optional number of bytes including terminating '\x01' byte
        +---------+-----------
        |0000 0010|                             0x02
        +---------+----------------------------
        |          element 1                       
        +--------------------------------------
        |          element 2                       
        +--------------------------------------
        .    .    .
        .    .    .
        .    .    .
        +--------------------------------------
        |          element N                       
        +---------+----------------------------
        |0000 0001|                             0x01
        +---------+

其中，list开头字节为 0x02, list结尾字节为0x01, list的元素可以是VBS支持的任意
类型，包括list或dict。


dict类型
-------------

dict类型数据编码成 dict开头字节 + dict所有键值对 + 结尾字节。如下图所示::

        +---------------------
        | optional number of bytes including terminating '\x01' byte
        +---------+
        |0000 0011|                             0x03
        +---------+----------------------------
        |            key 1   
        +--------------------------------------
        |          value 1
        +--------------------------------------
        |            key 2   
        +--------------------------------------
        |          value 2
        +--------------------------------------
        .    .    .
        .    .    .
        .    .    .
        +--------------------------------------
        |            key N   
        +--------------------------------------
        |          value N
        +---------+----------------------------
        |0000 0001|                             0x01
        +---------+

其中，dict开头字节为 ``0x03``, dict结尾字节为 ``0x01`` (与list结尾字节一样), 
dict的键值对的键和值可以是VBS支持的任意类型，包括list或dict。


null类型
---------------

null类型只有一个值就是null。null编码成一个字节。如下图所示::

        +---------+  
        |0000 1111|                             0x0F
        +---------+

null类型的引入是为了和JSON保持一致，通常其用来表示一个值不存在。


descriptor描述符
-----------------------

descriptor描述符不是一种类型，它是附加在某种数据类型前面的一个特定值，
对该类型数据起到描述，限定，补充等作用。描述符是可选的，数据前可以有
描述符，也可以没有。描述符分为普通描述符和特殊描述符。普通描述符用一个
15比特位的正整数表示，最多有32767个值。特殊描述符只有一个值。
描述符的含义由应用自行解释，VBS编码不对描述符的值做任何解释。

目前VBS编码中，一项数据前，最多只能附加一个普通描述符和一个特殊描述符。 
普通描述符按照下图的方式进行编码::

        LSB                                 MSB
        +---------+---------+.......+---------+
        |1xxx xxxx|1xxx xxxx|       |0001 0xxx| 
        +---------+---------+.......+---------+

特殊描述符编码为::

        +---------+
        |0001 0000|                             0x10
        +---------+

描述符的可以有多种用处，比如，用来表示某种数据类型的子类型。举个例子，
如果我们想用整形数据来表示时间戳，但是又觉得这个表示时间戳的整数和普通
的整数是有区别的（比如取值范围不同），那么我们可以加一个描述符，用来
指明这是一个表示时间戳的整数，相当于整数的一种子类型。在应用中，我们就
可以对这种表示时间戳的整数进行额外的检查，此外，打印的时候也可以采用特定
的方式。描述符的另一种用法是用来编码一些标志位，在应用中，我们可以根据
这些标志位对该数据进行对应的特定处理。用描述符表示标志位时，应尽量将常用
的标志位放在低位，不常用的放在高位，使描述符的编码尽可能的短。VBS编码并
没有对描述符的使用做任何限制，完全取决于应用本身。


VBS数据的文本表示
===================

编码后的VBS数据是一种二进制的格式，为了调试的方便，我们通常需要将其打印成文本
格式，因此我们定义了一种VBS的文本表示方法。这种方法和VBS编码本身没有关系，应用
程序可以采用任何方法来将VBS数据表示成文本格式，以方便调试。但如果采用统一的
方式，会更有利于沟通交流。

这种表示方法以简短明确为原则，尽量使文本更简短，使解析更方便，特殊字符的转义
方式尽量不和常见的转义方式相冲突。


VBS数据值的表示
--------------------

整数表示为 ``12345`` 或 ``0x3039``

浮点数表示为 ``1.2345`` 或 ``.12345`` 或 ``12345.`` 或 ``1.2345E4``

十进制浮点数表示为 ``0D``, ``1.2345D`` 或 ``.12345D`` 或 ``12345D`` 或 ``1.2345E4D``

布尔值表示为 ``~T`` (真) ``~F`` (假)

字符串表示为字符串本身, 比如 hello 表示为 hello (没有引号), 
字符串中的特殊字符 ::

        \x00-\x1f  \x7f  \xff  ^~`;[]{}

需要进行转义，转义方式类似于URL编码，但将 '%' 换为 '`' 。
如果字符串的第一个字符不为ASCII字母或最后一个字符不是ASCII的可见字符，则在
字符串文本前面以 ``~!`` 打头, 后面以 ~ 结尾。比如 ``~!012345abc~``

blob表示类似于字符串，对特殊字符也采用 '`' 转义，不同的是blob前面总是以 ``~|``
打头, 后面以 ~ 结尾。

null表示为 ``~N``

list的开头表示为 [, 结尾表示为 ]

dict的开头表示为 {, 结尾表示为 }


VBS数据类型的表示
----------------------

::

        整数类型                %i
        二进制浮点数类型        %f
        十进制浮点数类型        %d
        布尔类型                %t
        字符串类型              %s
        blob类型                %b
        null类型                %n
        list类型                []
        dict类型                {}
        基本类型(ifdtSB)        %x              # ifdtsb
        所有类型                %X              # 基本类型+null+list+dict


示例
---------

下面这个VBS文本表示一个dict, 该dict里有3个键值对，第1个键为字符串"from", 
值为整数12345, 第2个键为字符串"body", 值为字符串"hello, world!", 第3个键
为字符串"time", 值为整数:1291715602。::

        {from^12345; body^hello, world!; time^1291715602;}

下面这个VBS文本表示一个dict, 该dict里有2个键值对，第1个键为字符串"fields", 
值为一个list(该list有3个元素，全部为字符串, 分别是"id", "name", "ok")，
第2个键为字符串"rows", 值为一个list(该list有2个元素，每个元素又是一个list，
第1个list的元素分别为整数1，字符串"Alice", 布尔值True)。::

        {fields^[id; name; ok]; rows^[[1; Alice; ~T]; [2; Bob; ~F]]}

下面这个VBS文本表示一个dict, 该dict有4个键值对，第1个键为字符串"from",
值为整数类型的数据，第2个键为字符串"body", 值为字符串类型，...... ::

        {from^%i; body^%s; time^%i; subst^{%s^{type^%i; data^%x}}}

下面这个VBS文本表示一个dict, 该dict有2个键值对，第1个键为字符串"fields",
值为list(该list的元素类型为字符串)，第2个键为字符串"rows"，值为list
(该list的值为list(该list的值为整数，浮点数，字符串或blob的任一种))。 ::

        {fields^[%s]; rows^[[%x]]}


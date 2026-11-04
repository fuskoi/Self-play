// 站点配置 —— 改密码请看文件底部说明
window.SITE_CONFIG = {
  title: "我的资源库",
  subtitle: "私人链接 · 仅自己可见",
  // 当前密码：147369
  passwordHash:
    "bc1583dbd69cf369314be86b8579f367cd17e441986f9131c240015829f044cf",
};

/*
  如何修改密码：
  1. 用浏览器打开本站并解锁
  2. 按 F12 打开控制台
  3. 若提示不能粘贴，先输入（英文，不要加引号）：
       allow pasting
     再回车
  4. 再输入（注意拼写和大小写）：
       await hashPassword("你的新密码")
  5. 把打印出的一长串字符，粘贴替换上面的 passwordHash
  6. 保存后重新上传 / 部署
*/

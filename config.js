// 站点配置
window.SITE_CONFIG = {
  title: "我的资源库",
  subtitle: "私人链接 · 全设备同步",
  // 当前密码：147369
  passwordHash:
    "bc1583dbd69cf369314be86b8579f367cd17e441986f9131c240015829f044cf",

  /*
    全设备同步（必填后才能「一台改、全部看到」）
    1. 打开 https://jsonbin.io 注册（免费）
    2. 右上角头像 → API Keys → 复制 X-Master-Key
    3. 点 Create Bin，内容先填：{"version":1,"extras":[],"overrides":{},"deleted":[]}
    4. 创建后地址栏或详情里的 Bin ID 复制下来
    5. 填到下面 remote.binId 和 remote.apiKey
    6. 重新发布网站
  */
  remote: {
    binId: "6aeb02aaac6210605a6b57da",
    apiKey: "$2a$10$HISJzcLILRM6dmQ1KgCdDuHTW0i1edRLe4xCWp5QPN5cAtQjNn8p.",
  },
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

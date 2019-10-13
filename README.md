# Detector

### 请求

调用地址：http://server/detect
请求方式：POST
请求类型：image/jpeg

| 将图片的二进制数据放入request的body内，然后进行post |

### 返回

返回类型：JSON

| 参数名 | 类型 | 参数说明 |
| :----: | :--: | :------: |
| code | `Int` | http状态码 |
| box | `Int Array` | 四个数字的array,'''[a,b,c,d]'''表示[left, upper, right, lower]|

### 成功请求返回值示例

```json
{
	"code": 200,
  	"box": [1,2,3,4]
}
```

### 2.1.4 失败请求返回值示例

```json
{
	"code": 406
}
```

`wget "https://ai.arcsoft.com.cn/packages/arcface/linux/ArcSoft_ArcFace_Linux_x64_V2.2.zip" -O sdk.zip`

`unzip -j sdk.zip '*/*.so' -d linux_so/`

`rm -f sdk.zip`
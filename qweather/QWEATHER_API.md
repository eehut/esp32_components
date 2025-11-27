# 和风天气 API 使用文档

## 认证方式

和风 API 使用 JSON Web Token (JWT) 进行身份认证。

详细文档请参考：[和风API认证文档](https://dev.qweather.com/docs/configuration/authentication/#json-web-token)

### Python 示例

```python
#!/usr/bin/env python3
import sys
import time
import jwt

# Open PEM
private_key = """YOUR_PRIVATE_KEY"""

payload = {
    'iat': int(time.time()) - 30,
    'exp': int(time.time()) + 900,
    'sub': 'YOUR_PROJECT_ID'
}
headers = {
    'kid': 'YOUR_KEY_ID'
}

# Generate JWT
encoded_jwt = jwt.encode(payload, private_key, algorithm='EdDSA', headers=headers)

print(f"JWT:  {encoded_jwt}")
```

## 获取实时天气 API

### 请求示例

```shell
curl -X GET --compressed -H 'Authorization: Bearer your_token' 'https://your_api_host/v7/weather/now?location=101010100'
```

> **注意**: 其中 `Bearer your_token` 就是上面提到的 JWT 编码结果。

## 地区查询

### 请求示例

```shell
curl -X GET --compressed \
  -H 'Authorization: Bearer your_token' \
  'https://your_api_host/geo/v2/city/lookup?location=beij'
```

> **注意**: 请将 `your_token` 替换为你的 JWT 身份认证，将 `your_api_host` 替换为你的 API Host。

### 返回数据

返回数据是 JSON 格式并进行了 Gzip 压缩。

```json
{
  "code": "200",
  "location": [
    {
      "name": "北京",
      "id": "101010100",
      "lat": "39.90499",
      "lon": "116.40529",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "10",
      "fxLink": "https://www.qweather.com/weather/beijing-101010100.html"
    },
    {
      "name": "海淀",
      "id": "101010200",
      "lat": "39.95607",
      "lon": "116.31032",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "15",
      "fxLink": "https://www.qweather.com/weather/haidian-101010200.html"
    },
    {
      "name": "朝阳",
      "id": "101010300",
      "lat": "39.92149",
      "lon": "116.48641",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "15",
      "fxLink": "https://www.qweather.com/weather/chaoyang-101010300.html"
    },
    {
      "name": "昌平",
      "id": "101010700",
      "lat": "40.21809",
      "lon": "116.23591",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "23",
      "fxLink": "https://www.qweather.com/weather/changping-101010700.html"
    },
    {
      "name": "房山",
      "id": "101011200",
      "lat": "39.73554",
      "lon": "116.13916",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "23",
      "fxLink": "https://www.qweather.com/weather/fangshan-101011200.html"
    },
    {
      "name": "通州",
      "id": "101010600",
      "lat": "39.90249",
      "lon": "116.65860",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "23",
      "fxLink": "https://www.qweather.com/weather/tongzhou-101010600.html"
    },
    {
      "name": "丰台",
      "id": "101010900",
      "lat": "39.86364",
      "lon": "116.28696",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "25",
      "fxLink": "https://www.qweather.com/weather/fengtai-101010900.html"
    },
    {
      "name": "大兴",
      "id": "101011100",
      "lat": "39.72891",
      "lon": "116.33804",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "25",
      "fxLink": "https://www.qweather.com/weather/daxing-101011100.html"
    },
    {
      "name": "延庆",
      "id": "101010800",
      "lat": "40.46532",
      "lon": "115.98501",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "33",
      "fxLink": "https://www.qweather.com/weather/yanqing-101010800.html"
    },
    {
      "name": "平谷",
      "id": "101011500",
      "lat": "40.14478",
      "lon": "117.11234",
      "adm2": "北京",
      "adm1": "北京市",
      "country": "中国",
      "tz": "Asia/Shanghai",
      "utcOffset": "+08:00",
      "isDst": "0",
      "type": "city",
      "rank": "33",
      "fxLink": "https://www.qweather.com/weather/pinggu-101011500.html"
    }
  ],
  "refer": {
    "sources": [
      "QWeather"
    ],
    "license": [
      "QWeather Developers License"
    ]
  }
}
```

### 返回字段说明

#### 通用字段

- `code` - 请参考状态码

#### location 对象字段

- `location.name` - 地区/城市名称
- `location.id` - 地区/城市ID
- `location.lat` - 地区/城市纬度
- `location.lon` - 地区/城市经度
- `location.adm2` - 地区/城市的上级行政区划名称
- `location.adm1` - 地区/城市所属一级行政区域
- `location.country` - 地区/城市所属国家名称
- `location.tz` - 地区/城市所在时区
- `location.utcOffset` - 地区/城市目前与UTC时间偏移的小时数，参考详细说明
- `location.isDst` - 地区/城市是否当前处于夏令时。`1` 表示当前处于夏令时，`0` 表示当前不是夏令时
- `location.type` - 地区/城市的属性
- `location.rank` - 地区评分
- `location.fxLink` - 该地区的天气预报网页链接，便于嵌入你的网站或应用

#### refer 对象字段

- `refer.sources` - 原始数据来源，或数据源说明，可能为空
- `refer.license` - 数据许可或版权声明，可能为空


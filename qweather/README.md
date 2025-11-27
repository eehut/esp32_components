
# 和风天气组件

## 支持的图标

支持显示100-999常用天气图标，图标大小为32x32.


## `test.py` 执行结果 

```sh

Generating JWT token...
JWT Token: eyJhbGciOiJFZERTQSIsImtpZCI6IkM5R1lBQjJLMkUiLCJ0eXAiOiJKV1QifQ.eyJpYXQiOjE3NjM4ODg4ODIsImV4cCI6MTc2Mzg4OTgxMiwic3ViIjoiMk0yQkVHUDNXQSJ9.FPCtJ7y8Omnn493nZNkqHtIiXhKxMnYL4LQwzl_fb_KPk8kys6u3mL5GBz8R5BipLaHe58JmKzPOG49a6xx3Cg

Requesting weather data for location: 101280604
Successfully retrieved weather data:
{
  "code": "200",
  "updateTime": "2025-11-23T17:02+08:00",
  "fxLink": "https://www.qweather.com/weather/nanshan-101280604.html",
  "now": {
    "obsTime": "2025-11-23T17:02+08:00",
    "temp": "24",
    "feelsLike": "25",
    "icon": "100",
    "text": "晴",
    "wind360": "135",
    "windDir": "东南风",
    "windScale": "1",
    "windSpeed": "2",
    "humidity": "53",
    "precip": "0.0",
    "pressure": "1010",
    "vis": "26",
    "cloud": "8",
    "dew": "8"
  },
  "refer": {
    "sources": [
      "QWeather"
    ],
    "license": [
      "QWeather Developers License"
    ]
  }
}

=== Current Weather ===
Temperature: 24°C
Feels Like: 25°C
Weather: 晴
Humidity: 53%
Wind: 东南风 1级

```

## 天气组件封装需求

天气组件配置，默认为空，无法执行请求，需要配置后才可以使用，掉电保存。
typedef struct QWeatherConfig
{
    char project_id[]
    char key_id[]
    char api_host[];
}qweather_config_t;

天气信息，建议通过app_event 发出来。
typedef struct QWeaherInfo
{
     bool valid; //是否有效
     int  status_code; // 200 表示成功，一般API会返回http 的代码，其他错误如 配置无效，网络错误等，使用自定义数字作为错误码(与HTTP code兼容)。
     uint32_t location_code; // 地区， 来自配置 
     float temperature;
     float humidity;
     char weather_text[]; //如 多云
     uint16_t weather_icon;
     uint32_t update_time;// 更新时间
}qweaher_info_t;

天气组件导出函数

qweather_get_config();
qweather_set_config();

// 请求更新天气，异步方式，结果使用 app_event发出来
qweather_query_async(uint32_t location_code); 
// 同步方式 
qweather_query(uint32_t location_code, qweather_info_t *info);

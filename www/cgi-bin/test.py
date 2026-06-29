#!/usr/bin/env python3
import sys
import os

# طباعة الهيدرز الإلزامية لـ HTTP
print("Content-Type: text/plain")
print("Status: 200 OK")
print("") # سطر فارغ إلزامي يفصل الهيدرز عن الـ Body

# طباعة المحتوى (Body)
print("Hello from CGI script!")
print(f"Request Method received: {os.environ.get('REQUEST_METHOD')}")
print(f"Query String received: {os.environ.get('QUERY_STRING')}")

# إذا كان الطلب POST، سنقرأ البيانات القادمة من الـ Stdin
if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    if content_length > 0:
        body_data = sys.stdin.read(content_length)
        print(f"Post Body received: {body_data}")
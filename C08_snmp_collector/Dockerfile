FROM python:3.9-slim
WORKDIR /app
COPY collector.py .
RUN pip install pymongo psutil
CMD ["python", "collector.py"]

#!/bin/bash

# P5.js 传感器网络可视化系统启动脚本

echo "========================================"
echo "  P5.js 传感器网络可视化系统"
echo "========================================"
echo ""

# 检查 Node.js 是否安装
if ! command -v node &> /dev/null; then
    echo "❌ Node.js 未安装，请先安装 Node.js"
    exit 1
fi

echo "✅ Node.js 版本: $(node --version)"

# 检查依赖是否安装
if [ ! -d "node_modules" ]; then
    echo "📦 正在安装依赖..."
    npm install --cache /tmp/.npm
    if [ $? -ne 0 ]; then
        echo "❌ 依赖安装失败"
        exit 1
    fi
    echo "✅ 依赖安装完成"
fi

echo ""
echo "🚀 启动服务器..."
echo "📱 可视化界面: http://localhost:3000"
echo "🔧 API 状态: http://localhost:3000/api/status"
echo ""
echo "按 Ctrl+C 停止服务器"
echo "========================================"
echo ""

# 启动服务器
node server.js

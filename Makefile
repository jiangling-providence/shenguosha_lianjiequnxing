# ============================================
#  KillGame  Makefile (with ONNX Runtime AI)
#  Windows + MinGW-w64 (MSYS2) + SDL2 + ONNX Runtime
# ============================================

# 编译器
CC = gcc

# ONNX Runtime路径
ONNX_DIR = onnxruntime-win-x64-1.19.2

# ====== 编译参数 ======
CFLAGS = -Wall -g -O2 -I. -Iheroes -Iheroes/feixiao -Iheroes/zhaoyun -Iheroes/gilgamesh -Iheroes/linyuxia -Iheroes/paladin -Iheroes/yudie -Iheroes/liuying -Iheroes/jingliu \
         -I$(ONNX_DIR)/include

# ====== 链接参数 ======
LDFLAGS = -L$(ONNX_DIR)/lib \
          -lonnxruntime -lonnxruntime_providers_shared \
          -lmingw32 -lSDL2main -lSDL2 -lSDL2_ttf -lSDL2_image -lm

# ====== 源文件 ======
SRCS = main.c \
       card.c \
       player.c \
       game.c \
       render.c \
       input.c \
       state_encoder.c \
       nn_bridge.c \
       heroes/hero.c \
       heroes/feixiao/feixiao.c \
       heroes/zhaoyun/zhaoyun.c \
       heroes/gilgamesh/gilgamesh.c \
       heroes/linyuxia/linyuxia.c \
       heroes/paladin/paladin.c \
       heroes/yudie/yudie.c \
       heroes/liuying/liuying.c \
       heroes/jingliu/jingliu.c

OBJS = $(SRCS:.c=.o)

TARGET = killgame.exe

# ====== 默认目标 ======
all: $(TARGET)

# ====== 链接 ======
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)
	@echo ""
	@echo "  编译成功！生成 $(TARGET)"
	@echo ""
	@echo "  运行需要以下文件在同一目录："
	@echo "  - SDL2.dll, SDL2_ttf.dll, SDL2_image.dll"
	@echo "  - onnxruntime.dll, onnxruntime_providers_shared.dll"
	@echo "  - model.onnx（神经网络模型）"
	@echo "  - res/ 资源文件夹"
	@echo ""

# ====== 编译 ======
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ====== 清理 ======
clean:
	rm -f $(OBJS) $(TARGET)

# ====== 运行 ======
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run

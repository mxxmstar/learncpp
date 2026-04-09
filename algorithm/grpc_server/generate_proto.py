# 生成 Python gRPC 代码脚本（视频处理服务）
import subprocess
import sys
import os

# 路径配置
PROTO_FILE = "../../grpc/proto/video_processing.proto"
OUTPUT_DIR = "."

def generate_grpc_code():
    """生成 Python gRPC 代码"""
    print("Generating Python gRPC code for video processing...")
    
    # 检查 proto 文件是否存在
    if not os.path.exists(PROTO_FILE):
        print(f"Error: Proto file not found: {PROTO_FILE}")
        sys.exit(1)
    
    try:
        # 生成 protobuf 和 gRPC 代码
        print("Generating protobuf and gRPC code...")
        subprocess.run([
            sys.executable, "-m", "grpc_tools.protoc",
            f"--proto_path={os.path.dirname(PROTO_FILE)}",
            f"--python_out={OUTPUT_DIR}",
            f"--grpc_python_out={OUTPUT_DIR}",
            PROTO_FILE
        ], check=True)
        
        print("✓ Code generation successful!")
        print(f"Generated files in: {OUTPUT_DIR}")
        
        # 列出生成的文件
        for file in sorted(os.listdir(OUTPUT_DIR)):
            if file.endswith('_pb2.py') or file.endswith('_pb2_grpc.py'):
                print(f"  - {file}")
        
    except subprocess.CalledProcessError as e:
        print(f"✗ Code generation failed: {e}")
        sys.exit(1)
    except Exception as e:
        print(f"✗ Error: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == "__main__":
    generate_grpc_code()

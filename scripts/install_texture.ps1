# Texture Installation Script for Moon Engine
# 用法: .\install_texture.ps1 -ZipPath "下载的ZIP路径" -MaterialName "材质名称"
# 示例: .\install_texture.ps1 -ZipPath "C:\Users\Administrator\Downloads\wood_floor_2k.zip" -MaterialName "WoodFloor"

param(
    [Parameter(Mandatory=$true)]
    [string]$ZipPath,
    
    [Parameter(Mandatory=$true)]
    [string]$MaterialName
)

$ErrorActionPreference = "Stop"

# 检查 ZIP 文件是否存在
if (!(Test-Path $ZipPath)) {
    Write-Host "❌ 错误: ZIP 文件不存在: $ZipPath" -ForegroundColor Red
    exit 1
}

# 临时解压目录
$tempDir = "$env:TEMP\moon_texture_temp_$([guid]::NewGuid().ToString('N').Substring(0,8))"
$targetDir = "assets\textures\materials\${MaterialName}_2K-PNG"

try {
    Write-Host "`n🔧 开始安装材质: $MaterialName" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    
    # 1. 解压 ZIP 文件
    Write-Host "`n📦 正在解压 ZIP 文件..." -ForegroundColor Yellow
    Expand-Archive -Path $ZipPath -DestinationPath $tempDir -Force
    
    # 2. 查找所有 PNG 文件
    $pngFiles = Get-ChildItem -Path $tempDir -Recurse -Filter "*.png" -File
    
    if ($pngFiles.Count -eq 0) {
        throw "未找到任何 PNG 文件"
    }
    
    Write-Host "   找到 $($pngFiles.Count) 个 PNG 文件" -ForegroundColor Green
    
    # 3. 创建目标目录
    if (!(Test-Path $targetDir)) {
        New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        Write-Host "   创建目录: $targetDir" -ForegroundColor Green
    }
    
    # 4. 文件映射规则 (Poly Haven 常见命名)
    $fileMapping = @{
        # Diffuse/Color
        "_diff_"   = "${MaterialName}_2K-PNG_Color.png"
        "_diffuse_" = "${MaterialName}_2K-PNG_Color.png"
        "_color_"  = "${MaterialName}_2K-PNG_Color.png"
        "_basecolor_" = "${MaterialName}_2K-PNG_Color.png"
        
        # Normal
        "_nor_dx_" = "${MaterialName}_2K-PNG_NormalDX.png"
        "_normal_dx_" = "${MaterialName}_2K-PNG_NormalDX.png"
        "_normaldx_" = "${MaterialName}_2K-PNG_NormalDX.png"
        
        # ARM (AO/Rough/Metal combined)
        "_arm_"    = @(
            "${MaterialName}_2K-PNG_AmbientOcclusion.png",
            "${MaterialName}_2K-PNG_Roughness.png",
            "${MaterialName}_2K-PNG_Metalness.png"
        )
        
        # Individual maps
        "_ao_"     = "${MaterialName}_2K-PNG_AmbientOcclusion.png"
        "_rough_"  = "${MaterialName}_2K-PNG_Roughness.png"
        "_metal_"  = "${MaterialName}_2K-PNG_Metalness.png"
        "_metalness_" = "${MaterialName}_2K-PNG_Metalness.png"
    }
    
    # 5. 智能重命名并复制
    Write-Host "`n📝 正在复制和重命名文件..." -ForegroundColor Yellow
    
    $copiedFiles = @()
    foreach ($file in $pngFiles) {
        $fileName = $file.Name.ToLower()
        $matched = $false
        
        foreach ($pattern in $fileMapping.Keys) {
            if ($fileName -like "*$pattern*") {
                $targetNames = $fileMapping[$pattern]
                
                # 处理单个目标文件
                if ($targetNames -is [string]) {
                    $targetPath = Join-Path $targetDir $targetNames
                    Copy-Item -Path $file.FullName -Destination $targetPath -Force
                    Write-Host "   ✓ $($file.Name) → $targetNames" -ForegroundColor Green
                    $copiedFiles += $targetNames
                    $matched = $true
                    break
                }
                # 处理多个目标文件 (ARM)
                else {
                    foreach ($targetName in $targetNames) {
                        $targetPath = Join-Path $targetDir $targetName
                        Copy-Item -Path $file.FullName -Destination $targetPath -Force
                        Write-Host "   ✓ $($file.Name) → $targetName" -ForegroundColor Green
                        $copiedFiles += $targetName
                    }
                    $matched = $true
                    break
                }
            }
        }
        
        if (!$matched) {
            Write-Host "   ⚠ 跳过未识别的文件: $($file.Name)" -ForegroundColor DarkYellow
        }
    }
    
    # 6. 验证必需文件
    Write-Host "`n🔍 验证材质文件完整性..." -ForegroundColor Yellow
    
    $requiredFiles = @(
        "${MaterialName}_2K-PNG_Color.png",
        "${MaterialName}_2K-PNG_NormalDX.png"
    )
    
    $missing = @()
    foreach ($req in $requiredFiles) {
        if (!(Test-Path (Join-Path $targetDir $req))) {
            $missing += $req
        }
    }
    
    if ($missing.Count -gt 0) {
        Write-Host "   ⚠ 警告: 缺少以下文件:" -ForegroundColor Yellow
        foreach ($m in $missing) {
            Write-Host "      - $m" -ForegroundColor Yellow
        }
    } else {
        Write-Host "   ✓ 核心文件完整" -ForegroundColor Green
    }
    
    # 7. 显示最终结果
    Write-Host "`n📊 安装完成统计:" -ForegroundColor Cyan
    Write-Host "   材质目录: $targetDir" -ForegroundColor Gray
    $installedFiles = Get-ChildItem -Path $targetDir -File
    foreach ($f in $installedFiles) {
        $sizeKB = [math]::Round($f.Length / 1KB, 1)
        Write-Host "   ✓ $($f.Name) (${sizeKB} KB)" -ForegroundColor Green
    }
    
    Write-Host "`n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    Write-Host "✅ 材质 '$MaterialName' 安装成功!" -ForegroundColor Green
    Write-Host "`n💡 下一步:" -ForegroundColor Cyan
    Write-Host "   1. 在 CSG JSON 中使用材质: `"material`": `"$($MaterialName.ToLower())`"" -ForegroundColor Gray
    Write-Host "   2. 运行引擎查看渲染效果: .\bin\x64\Debug\HelloEngine.exe" -ForegroundColor Gray
    
} catch {
    Write-Host "`n❌ 安装失败: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
} finally {
    # 清理临时文件
    if (Test-Path $tempDir) {
        Remove-Item -Path $tempDir -Recurse -Force -ErrorAction SilentlyContinue
    }
}

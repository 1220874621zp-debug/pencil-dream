<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="vi">
<context>
    <name>AboutDialog</name>
    <message>
        <location filename="../app/ui/aboutdialog.ui" line="26"/>
        <source>About</source>
        <comment>About Dialog Window Title</comment>
        <translation>Thông tin phần mềm</translation>
    </message>
    <message>
        <location filename="../app/ui/aboutdialog.ui" line="52"/>
        <source>Developed by: &lt;b&gt;Pascal Naidon, Patrick Corrieri, Matt Chang&lt;/b&gt;&lt;br&gt;Thanks to Qt Framework&lt;br&gt;Distributed under the GNU General Public License, version 2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Official site: &lt;a href=&quot;https://www.pencil2d.org&quot;&gt;pencil2d.org&lt;/a&gt;&lt;br&gt;Developed by: &lt;b&gt;Pascal Naidon, Patrick Corrieri, Matt Chang&lt;/b&gt;&lt;br&gt;Thanks to Qt Framework &lt;a href=&quot;https://www.qt.io/download&quot;&gt;https://www.qt.io/&lt;/a&gt;&lt;br&gt;miniz: &lt;a href=&quot;https://github.com/richgel999/miniz&quot;&gt;https://github.com/richgel999/miniz&lt;/a&gt;&lt;br&gt;Distributed under the &lt;a href=&quot;http://www.gnu.org/licenses/gpl-2.0.html&quot;&gt;GNU General Public License, version 2&lt;/a&gt;</source>
        <translation type="vanished">Trang chủ chính thức: &lt;a href=&quot;https://www.pencil2d.org&quot;&gt;pencil2d.org&lt;/a&gt;&lt;br&gt;Nhà phát triển: &lt;b&gt;Pascal Naidon, Patrick Corrieti, Matt Chang&lt;/b&gt;&lt;br&gt;Sử dụng Qt Framework &lt;a href=&quot;https://www.qt.io/download&quot;&gt;https://www.qt.io/&lt;/a&gt;&lt;br&gt;miniz: &lt;a href=&quot;https://github.com/richgel999/miniz&quot;&gt;https://github.com/richgel999/miniz&lt;/a&gt;&lt;br&gt;Được phát hành tuân thủ theo &lt;a href=&quot;http://www.gnu.org/licenses/gpl-2.0.html&quot;&gt;Giấy phép Công cộng GNU, bản thứ 2&lt;/a&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/aboutdialog.cpp" line="53"/>
        <source>Version: %1</source>
        <comment>Version Number in About Dialog</comment>
        <translation>Phiên bản: %1</translation>
    </message>
    <message>
        <location filename="../app/src/aboutdialog.cpp" line="76"/>
        <source>Copy to clipboard</source>
        <comment>Copy system info from About Dialog</comment>
        <translation>Sao chép vào clipboard</translation>
    </message>
</context>
<context>
    <name>ActionCommands</name>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="121"/>
        <source>Importing movie...</source>
        <translation>Đang nhập phim...</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="87"/>
        <location filename="../app/src/actioncommands.cpp" line="121"/>
        <location filename="../app/src/actioncommands.cpp" line="244"/>
        <location filename="../app/src/actioncommands.cpp" line="471"/>
        <source>Abort</source>
        <translation>Hủy</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="87"/>
        <source>Importing Animated Image...</source>
        <translation>Đang nhập ảnh động...</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="129"/>
        <source>You are importing a lot of frames, beware this could take some time. Are you sure you want to proceed?</source>
        <translation>Bạn đang nhập khá nhiều khung hình, việc này sẽ mất một chút thời gian. Bạn có chắc chắn muốn tiếp tục không?</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="177"/>
        <source>No sound layer exists as a destination for your import. Create a new sound layer?</source>
        <translation>Không tìm thấy Layer âm thanh cho dữ liệu bạn đã nhập. Bạn có muốn tạo một Layer âm thanh mới không?</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="178"/>
        <source>Create sound layer</source>
        <translation>Tạo một Layer âm thanh mới</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="179"/>
        <source>Don&apos;t create layer</source>
        <translation>Đừng tạo Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="189"/>
        <source>Layer Properties</source>
        <comment>Dialog title on creating a sound layer</comment>
        <translation>Thông tin Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="190"/>
        <location filename="../app/src/actioncommands.cpp" line="928"/>
        <location filename="../app/src/actioncommands.cpp" line="941"/>
        <location filename="../app/src/actioncommands.cpp" line="954"/>
        <location filename="../app/src/actioncommands.cpp" line="967"/>
        <source>Layer name:</source>
        <translation>Tên Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="191"/>
        <source>Sound Layer</source>
        <comment>Default name on creating a sound layer</comment>
        <translation>Layer Âm thanh</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="244"/>
        <source>Importing sound...</source>
        <translation>Đang nhập âm thanh...</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="285"/>
        <location filename="../app/src/actioncommands.cpp" line="490"/>
        <location filename="../app/src/actioncommands.cpp" line="592"/>
        <source>Something went wrong</source>
        <translation>Đã xảy ra sự cố</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="285"/>
        <location filename="../app/src/actioncommands.cpp" line="1111"/>
        <source>You currently have a total of %1 sound clips. Due to current limitations, you will be unable to export any animation exceeding %2 sound clips. We recommend splitting up larger projects into multiple smaller project to stay within this limit.</source>
        <translation>Bạn đang có tổng %1 clip có âm thanh. Do sự giới hạn tạm thời, bạn sẽ không thể xuất bất kì hiệu ứng nào vượt quá %2 clip âm thanh. Chúng tôi gợi ý chia nhỏ các dự án lớn thành nhiều dự án nhỏ khác nhau để giữ ở trong sự hạn chế.</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="343"/>
        <source>Exporting movie</source>
        <translation>Đang xuất phim...</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="387"/>
        <source>Finished. Open file location?</source>
        <translation>Đã hoàn tất. Mở vị trí tệp?</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="397"/>
        <source>Finished. Open movie now?</source>
        <comment>When movie export done.</comment>
        <translation>Đã hoàn tất. Mở phim ngay bây giờ?</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="405"/>
        <source>Unknown export error</source>
        <translation>Đã gặp lỗi xuất dữ liệu không xác định </translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="405"/>
        <source>The export did not produce any errors, however we can&apos;t find the output file. Your export may not have completed successfully.</source>
        <translation>Xuất dữ liệu không gặp lỗi, tuy nhiên chúng tôi không thể tìm thấy tệp đã xuất. Quá trình xuất của bạn có thể đã hoàn tất không thành công.</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="471"/>
        <source>Exporting image sequence...</source>
        <translation>Đang xuất chuỗi hình ảnh...</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="490"/>
        <source>Unable to export one or more images in the image sequence.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="718"/>
        <source>增加曝光</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="718"/>
        <source>减少曝光</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="761"/>
        <source>Insert frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="798"/>
        <source>删除选中帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="812"/>
        <source>Reverse frames</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="877"/>
        <source>Duplicate frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="898"/>
        <source>Move frame forward</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="915"/>
        <source>Move frame backward</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="942"/>
        <source>Colorize Layer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="1092"/>
        <location filename="../app/src/actioncommands.cpp" line="1111"/>
        <source>Warning</source>
        <translation>Cảnh báo</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="592"/>
        <source>Unable to export image.</source>
        <translation>Không xuất được hình ảnh.</translation>
    </message>
    <message>
        <source>Remove selected frames</source>
        <comment>Windows title of remove selected frames pop-up.</comment>
        <translation type="vanished">Xoá các khung hình đã chọn</translation>
    </message>
    <message>
        <source>Are you sure you want to remove the selected frames? This action is irreversible currently!</source>
        <translation type="vanished">Bạn có chắc bạn muốn xoá các khung hình đã chọn? Bạn sẽ không thể khôi phục chúng!</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="835"/>
        <source>%1 (copy)</source>
        <comment>Default duplicate layer name</comment>
        <translation>%1 (bản sao)</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="927"/>
        <location filename="../app/src/actioncommands.cpp" line="940"/>
        <location filename="../app/src/actioncommands.cpp" line="966"/>
        <source>Layer Properties</source>
        <translation>Thuộc tính của Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="929"/>
        <source>Bitmap Layer</source>
        <translation>Bitmap Layer</translation>
    </message>
    <message>
        <source>Vector Layer</source>
        <translation type="vanished">Vector Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="953"/>
        <source>Layer Properties</source>
        <comment>A popup when creating a new layer</comment>
        <translation>Đặc tính Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="955"/>
        <source>Camera Layer</source>
        <translation>Layer Máy quay</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="968"/>
        <source>Sound Layer</source>
        <translation>Layer âm thanh</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="987"/>
        <source>Delete Layer</source>
        <comment>Windows title of Delete current layer pop-up.</comment>
        <translation>Xóa Layer</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="988"/>
        <source>Are you sure you want to delete layer: %1? This cannot be undone.</source>
        <translation>Bạn có chắc bạn muốn xoá layer: %1? Bạn sẽ không thể hoàn tác.</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="997"/>
        <source>Please keep at least one camera layer in project</source>
        <comment>text when failed to delete camera layer</comment>
        <translation>Vui lòng giữ ít nhất một layer máy ảnh trong dự án của bạn</translation>
    </message>
    <message>
        <location filename="../app/src/actioncommands.cpp" line="1092"/>
        <source>The temporary directory is meant to be used only by Pencil2D. Do not modify it unless you know what you are doing.</source>
        <translation>Thư mục tạm thời này được tạo với mục đích sử dụng cho Pencil2D. Không chỉnh sửa trừ khi bạn biết mình đang làm gì.</translation>
    </message>
</context>
<context>
    <name>AddTransparencyToPaperDialog</name>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="14"/>
        <source>Replace Paper with Transparency</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="24"/>
        <source>Threshold</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="61"/>
        <source>Color values above this threshold will be made transparent</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="81"/>
        <source>Trace Red</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="109"/>
        <source>Trace Green</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="137"/>
        <source>Trace Blue</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="170"/>
        <source>Apply to:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="176"/>
        <source>Current Keyframe</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="183"/>
        <source>All Keyframes on Layer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="245"/>
        <source>Zoom</source>
        <translation>Thu phóng</translation>
    </message>
    <message>
        <location filename="../app/ui/addtransparencytopaperdialog.ui" line="265"/>
        <source>Test Transparency</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/addtransparencytopaperdialog.cpp" line="153"/>
        <source>Previewing Frame %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/addtransparencytopaperdialog.cpp" line="234"/>
        <source>Tracing scanned drawings...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/addtransparencytopaperdialog.cpp" line="234"/>
        <source>Abort</source>
        <translation>Hủy</translation>
    </message>
</context>
<context>
    <name>BaseTool</name>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="34"/>
        <source>Pencil</source>
        <translation>Bút chì</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="35"/>
        <source>Eraser</source>
        <translation>Tẩy xoá</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="36"/>
        <source>Select</source>
        <translation>Công cụ chọn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="37"/>
        <source>Move</source>
        <translation>Di chuyển</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="38"/>
        <source>Hand</source>
        <translation>Bàn tay</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="39"/>
        <source>Smudge</source>
        <translation>Làm mờ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="40"/>
        <source>Pen</source>
        <translation>Bút mực</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="41"/>
        <source>Polyline</source>
        <translation>Polyline</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="42"/>
        <source>Bucket</source>
        <translation>Xô màu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="43"/>
        <source>Eyedropper</source>
        <translation>Chọn màu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="44"/>
        <source>Brush</source>
        <translation>Cọ vẽ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="45"/>
        <source>Camera</source>
        <translation type="unfinished">Máy quay</translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="46"/>
        <source>洋葱皮对位</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="47"/>
        <source>Lasso</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/tool/basetool.cpp" line="48"/>
        <source>Deform</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>BrushOptionsWidget</name>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="64"/>
        <source>笔尖</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="67"/>
        <source>大小</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="68"/>
        <source>不透明度 %</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="69"/>
        <location filename="../app/src/brushoptionswidget.cpp" line="235"/>
        <source>流量 %</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="70"/>
        <source>硬度 %</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="71"/>
        <source>纵横比 %</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="72"/>
        <source>角度 °</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="82"/>
        <source>描边</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="85"/>
        <source>自动间距</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="86"/>
        <source>自动间距系数</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="87"/>
        <source>固定间距 %（直径比例）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="94"/>
        <source>动态</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="97"/>
        <source>压感</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="99"/>
        <source>防抖</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="101"/>
        <source>关</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="102"/>
        <source>弱</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="103"/>
        <source>中</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="104"/>
        <source>强</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="111"/>
        <source>高级</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="114"/>
        <source>散布 %（直径比例）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="115"/>
        <source>喷枪</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="116"/>
        <source>喷枪速率 /秒</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="120"/>
        <source>涂料模式</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="122"/>
        <source>涂抹（同笔不变深）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="123"/>
        <source>叠加（越描越深）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="125"/>
        <source>笔尖混合</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="128"/>
        <source>正常</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="129"/>
        <source>正片叠底</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="130"/>
        <source>滤色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="135"/>
        <source>镜像</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="136"/>
        <source>水平</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="137"/>
        <source>垂直</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushoptionswidget.cpp" line="235"/>
        <source>混合速率 %</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>BrushPresetPanel</name>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="68"/>
        <source>笔刷</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="83"/>
        <source>新建</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="84"/>
        <source>删除</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="85"/>
        <source>导入</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="86"/>
        <source>导出</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="88"/>
        <source>把当前画笔参数保存为新预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="89"/>
        <source>删除选中的用户预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="90"/>
        <source>从 .pbp 文件导入预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="91"/>
        <source>把选中的预设导出为 .pbp 文件</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="230"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="241"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="245"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="253"/>
        <source>新建笔刷预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="231"/>
        <source>预设名称:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="232"/>
        <source>我的笔刷</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="242"/>
        <source>“%1”是内置笔刷的名字，请换一个名字。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="246"/>
        <source>预设“%1”已存在，要覆盖它吗？</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="253"/>
        <source>预设保存失败。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="272"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="275"/>
        <source>删除笔刷预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="272"/>
        <source>内置笔刷不能删除。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="276"/>
        <source>确定删除预设“%1”吗？</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="288"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="295"/>
        <source>导入笔刷预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="290"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="314"/>
        <source>笔刷预设 (*.pbp)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="296"/>
        <source>无法读取该文件，可能不是有效的笔刷预设。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="312"/>
        <location filename="../app/src/brushpresetpanel.cpp" line="319"/>
        <source>导出笔刷预设</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="319"/>
        <source>预设导出失败。</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>BucketOptionsWidget</name>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="20"/>
        <source>Form</source>
        <translation>Bảng Mẫu</translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="54"/>
        <source>Reference</source>
        <translation>Tham khảo</translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="78"/>
        <source>Blend mode</source>
        <translation>Chế độ hòa trộn</translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="102"/>
        <source>模式</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="115"/>
        <source>连续区域：填充点击处的封闭区域；相似颜色：填充全图中颜色相近的所有区域（Shift+点击临时使用）；到边界色：填充直到指定颜色为止；选区填充：填充活动选区（Alt+点击临时使用）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="129"/>
        <source>边界色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="142"/>
        <source>到边界色模式停止填充的颜色，通常为线稿颜色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="156"/>
        <source>拖拽填充</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="169"/>
        <source>按住拖动时的填充行为：仅相似区域只填充与起笔处颜色相近的区域（适合线稿上色）；任意区域填充拖过的一切；不用则仅单击填充</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="254"/>
        <source>封闭线稿中不超过该像素数的缺口，防止填充物涌出（Krita 同款算法）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="274"/>
        <source>用高斯模糊羽化填充边缘</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="288"/>
        <source>边缘抗锯齿</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="291"/>
        <source>平滑填充边缘的锯齿（与羽化互斥，羽化优先）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="298"/>
        <source>扩展止于最深色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/bucketoptionswidget.ui" line="301"/>
        <source>扩展填充时遇到更深或更不透明的线稿像素即停止，避免越过线条</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="60"/>
        <source>Color tolerance</source>
        <translation>Dung sai màu</translation>
    </message>
    <message>
        <source>Expand fill</source>
        <translation type="vanished">Đổ tràn</translation>
    </message>
    <message>
        <source>Stroke thickness</source>
        <translation type="vanished">Độ dày nét vẽ</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="61"/>
        <source>扩展/收缩</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="62"/>
        <source>封闭间隙</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="63"/>
        <source>羽化</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="73"/>
        <source>Current layer</source>
        <comment>Reference Layer Options</comment>
        <translation>Layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="74"/>
        <source>All layers</source>
        <comment>Reference Layer Options</comment>
        <translation>Mọi layer</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="75"/>
        <source>Refers to the layer that used to flood fill from</source>
        <translation>Dựa vào tầng layer đã được làm tràn đầy từ...</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="77"/>
        <source>Overlay</source>
        <comment>Blend Mode dropdown option</comment>
        <translation>Lớp phủ</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="78"/>
        <source>Replace</source>
        <comment>Blend Mode dropdown option</comment>
        <translation>Thay thế</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="79"/>
        <source>Behind</source>
        <comment>Blend Mode dropdown option</comment>
        <translation>Sau</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="80"/>
        <source>Defines how the fill will behave when the new color is not opaque</source>
        <translation>Xác định cách chức năng đổ màu sẽ được thi hành khi màu mới thấu quang</translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="82"/>
        <source>连续区域</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="83"/>
        <source>相似颜色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="84"/>
        <source>到边界色</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="85"/>
        <source>选区填充</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="87"/>
        <source>仅相似区域</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="88"/>
        <source>任意区域</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="89"/>
        <source>不用</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/bucketoptionswidget.cpp" line="283"/>
        <source>选择边界色</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CameraContextMenu</name>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="30"/>
        <source>Easing: frame %1 to %2</source>
        <translation>Easing: khung hình %1 đến %2</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="36"/>
        <source>Selected: </source>
        <translation>Lựa chọn: </translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="40"/>
        <source>Linear</source>
        <translation>Tuyến tính (thẳng)</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="42"/>
        <source>In</source>
        <translation>Đầu</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="43"/>
        <source>Out</source>
        <translation>Cuối</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="44"/>
        <source>In-Out</source>
        <translation>Đầu-Cuối</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="45"/>
        <source>Out-In</source>
        <translation>Cuối-Đầu</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="47"/>
        <source>Slow</source>
        <translation>Chậm</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="48"/>
        <source>Moderate</source>
        <translation>Vừa phải</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="49"/>
        <source>Quick</source>
        <translation>Mau</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="50"/>
        <source>Fast</source>
        <translation>Nhanh</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="51"/>
        <source>Faster</source>
        <translation>Nhanh hơn</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="52"/>
        <source>Fastest</source>
        <translation>Nhanh nhất</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="53"/>
        <source>Circle-based</source>
        <translation>Có dạng tròn</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="54"/>
        <source>Overshoot</source>
        <translation>Vượt quá</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="55"/>
        <source>Elastic</source>
        <translation>Co giãn</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="56"/>
        <source>Bounce</source>
        <translation>Nảy</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="99"/>
        <source>Transform</source>
        <translation>Biến đổi</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="100"/>
        <source>Reset all</source>
        <translation>Đặt lại tất cả</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="102"/>
        <source>Reset position</source>
        <translation>Đặt lại vị trí </translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="103"/>
        <source>Reset scale</source>
        <translation>Đặt lại tỉ lệ</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="104"/>
        <source>Reset rotation</source>
        <translation>Đặt lại hướng xoay</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="106"/>
        <source>Align horizontally to frame %1</source>
        <translation>Căn chỉnh chiều ngang với khung hình %1</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="107"/>
        <source>Align vertically to frame %1</source>
        <translation>Căn chỉnh chiều dọc với khung hình %1</translation>
    </message>
    <message>
        <location filename="../app/src/cameracontextmenu.cpp" line="109"/>
        <source>Hold to keyframe %1</source>
        <translation>Giữ đến khung hình chính %1</translation>
    </message>
</context>
<context>
    <name>CameraEasingType</name>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="24"/>
        <source>Linear</source>
        <translation>Tuyến tính (thẳng)</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="25"/>
        <source>Moderate Ease-in</source>
        <translation>Ease-in vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="26"/>
        <source>Moderate Ease-out</source>
        <translation>Ease-out vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="27"/>
        <source>Moderate Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="28"/>
        <source>Moderate Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="29"/>
        <source>Quick Ease-in</source>
        <translation>Ease-in mau</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="30"/>
        <source>Quick Ease-out</source>
        <translation>Ease-out mau</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="31"/>
        <source>Quick Ease-in - Ease-out</source>
        <translation>Ease-in - Ease out mau</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="32"/>
        <source>Quick Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in mau</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="33"/>
        <source>Fast Ease-in</source>
        <translation>Ease-in nhanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="34"/>
        <source>Fast Ease-out</source>
        <translation>Ease-out nhanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="35"/>
        <source>Fast Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out nhanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="36"/>
        <source>Fast Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in nhanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="37"/>
        <source>Faster Ease-in</source>
        <translation>Ease-in nhanh hơn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="38"/>
        <source>Faster Ease-out</source>
        <translation>Ease-out nhanh hơn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="39"/>
        <source>Faster Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out nhanh hơn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="40"/>
        <source>Faster Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in nhanh hơn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="41"/>
        <source>Slow Ease-in</source>
        <translation>Ease-in chậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="42"/>
        <source>Slow Ease-out</source>
        <translation>Ease-out chậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="43"/>
        <source>Slow Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out chậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="44"/>
        <source>Slow Ease-out - Ease-in</source>
        <translation>Ease-out - Ease in chậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="45"/>
        <source>Fastest Ease-in</source>
        <translation>Ease-in nhanh nhất</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="46"/>
        <source>Fastest Ease-out</source>
        <translation>Ease-out nhanh nhất</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="47"/>
        <source>Fastest Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out nhanh nhất</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="48"/>
        <source>Fastest Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in nhanh nhất</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="49"/>
        <source>Circle-based Ease-in</source>
        <translation>Ease-in dạng tròn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="50"/>
        <source>Circle-based Ease-out</source>
        <translation>Ease-out dạng tròn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="51"/>
        <source>Circle-based Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out dạng tròn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="52"/>
        <source>Circle-based Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in dạng tròn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="53"/>
        <source>Elastic Ease-in</source>
        <translation>Co giãn vào trong</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="54"/>
        <source>Elastic Ease-out</source>
        <translation>Ease-out co giãn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="55"/>
        <source>Elastic Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out co giãn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="56"/>
        <source>Elastic Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in co giãn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="57"/>
        <source>Overshoot Ease-in</source>
        <translation>Ease-in vượt quá</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="58"/>
        <source>Overshoot Ease-out</source>
        <translation>Ease-out vượt quá</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="59"/>
        <source>Overshoot Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out vượt quá</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="60"/>
        <source>Overshoot Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in vượt quá</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="61"/>
        <source>Bounce Ease-in</source>
        <translation>Ease-in nảy</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="62"/>
        <source>Bounce Ease-out</source>
        <translation>Ease-out nảy</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="63"/>
        <source>Bounce Ease-in - Ease-out</source>
        <translation>Ease-in - Ease-out nảy</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/cameraeasingtype.cpp" line="64"/>
        <source>Bounce Ease-out - Ease-in</source>
        <translation>Ease-out - Ease-in nảy</translation>
    </message>
</context>
<context>
    <name>CameraOptionsWidget</name>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="35"/>
        <source>Transform</source>
        <translation>Biến đổi</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="56"/>
        <source>Reset scaling</source>
        <translation>Đặt lại tỉ lệ</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="73"/>
        <source>Reset rotation</source>
        <translation>Đặt lại hướng xoay</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="90"/>
        <source>Reset</source>
        <translation>Đặt lại</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="97"/>
        <source>Reset translation</source>
        <translation>Đặt lại bản dịch</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="114"/>
        <source>Reset all transforms</source>
        <translation>Đặt lại mọi biến đổi</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="117"/>
        <source>Reset all</source>
        <translation>Đặt lại tất cả</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="127"/>
        <source>Camera path</source>
        <translation>Đường đi máy quay</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="150"/>
        <source>Show interpolation path</source>
        <translation>Hiển thị đường dẫn nội suy</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="153"/>
        <source>Show path</source>
        <translation>Hiển thị đường dẫn</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="161"/>
        <source>Red</source>
        <translation>Đỏ</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="166"/>
        <source>Blue</source>
        <translation>Lục</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="171"/>
        <source>Green</source>
        <translation>Lam</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="176"/>
        <source>Black</source>
        <translation>Đen</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="181"/>
        <source>White</source>
        <translation>Trắng</translation>
    </message>
    <message>
        <location filename="../app/ui/cameraoptionswidget.ui" line="206"/>
        <location filename="../app/ui/cameraoptionswidget.ui" line="209"/>
        <source>Reset path</source>
        <translation>Đặt lại đường dẫn</translation>
    </message>
</context>
<context>
    <name>CameraPropertiesDialog</name>
    <message>
        <location filename="../app/ui/camerapropertiesdialog.ui" line="14"/>
        <source>Camera Properties</source>
        <translation>Thông tin Máy quay</translation>
    </message>
    <message>
        <location filename="../app/ui/camerapropertiesdialog.ui" line="22"/>
        <source>Camera name:</source>
        <translation>Tên máy quay:</translation>
    </message>
    <message>
        <location filename="../app/ui/camerapropertiesdialog.ui" line="36"/>
        <source>Camera size:</source>
        <translation>Kích thước máy quay:</translation>
    </message>
</context>
<context>
    <name>CheckUpdatesDialog</name>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="46"/>
        <source>Checking for Updates...</source>
        <comment>status description in the check-for-update dialog</comment>
        <translation>Đang kiểm tra bản cập nhật...</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="58"/>
        <source>Download</source>
        <translation>Tải về</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="59"/>
        <source>Close</source>
        <translation>Đóng</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="115"/>
        <source>&lt;b&gt;You are using a Pencil2D nightly build&lt;/b&gt;</source>
        <translation>&lt;b&gt;Bạn đang sử dụng Pencil2D bản nightly &lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="116"/>
        <source>Please go %1 here %2 to check new nightly builds.</source>
        <translation>Vui lòng đến %1 ở %2 để kiểm tra các bản nightly mới.</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="126"/>
        <location filename="../app/src/checkupdatesdialog.cpp" line="135"/>
        <location filename="../app/src/checkupdatesdialog.cpp" line="144"/>
        <source>&lt;b&gt;An error occurred while checking for updates&lt;/b&gt;</source>
        <comment>error msg of check-for-update</comment>
        <translation>&lt;b&gt;Đã xảy ra lỗi khi kiểm tra bản cập nhật mới&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="127"/>
        <source>Please check your internet connection and try again later.</source>
        <comment>error msg of check-for-update</comment>
        <translation>Vui lòng kiểm tra kết nối mạng của bạn và thử lại sau.</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="136"/>
        <source>Network response is empty</source>
        <comment>error msg of check-for-update</comment>
        <translation>Không có phản hồi mạng</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="145"/>
        <source>Couldn&apos;t retrieve the version information</source>
        <comment>error msg of check-for-update</comment>
        <translation>Không thể truy xuất thông tin của phiên bản</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="181"/>
        <source>&lt;b&gt;A new version of Pencil2D is available!&lt;/b&gt;</source>
        <translation>&lt;b&gt;Đã có một phiên bản mới của Pencil2D!&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="182"/>
        <source>Pencil2D %1 is now available -- you have %2. Would you like to download it?</source>
        <translation>Pencil2D %1 đã ra mắt -- bạn đang sử dụng %2. Bạn có muốn tải về phiên bản mới không?</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="189"/>
        <source>&lt;b&gt;Pencil2D is up to date&lt;/b&gt;</source>
        <translation>&lt;b&gt;Pencil2D đang ở bản mới nhất&lt;/b&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/checkupdatesdialog.cpp" line="190"/>
        <source>Version %1</source>
        <translation>Phiên bản %1</translation>
    </message>
</context>
<context>
    <name>ColorBox</name>
    <message>
        <location filename="../app/src/colorbox.cpp" line="26"/>
        <source>Color Box</source>
        <comment>Color Box window title</comment>
        <translation>Hộp màu</translation>
    </message>
</context>
<context>
    <name>ColorInspector</name>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="52"/>
        <source>HSV</source>
        <translation>Hệ màu HSV</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="70"/>
        <source>H</source>
        <translation>H</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="77"/>
        <source>S</source>
        <translation>S</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="84"/>
        <source>V</source>
        <translation>V</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="91"/>
        <location filename="../app/ui/colorinspector.ui" line="197"/>
        <source>A</source>
        <translation>A</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="138"/>
        <source>°</source>
        <translation>°</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="148"/>
        <location filename="../app/ui/colorinspector.ui" line="158"/>
        <location filename="../app/ui/colorinspector.ui" line="168"/>
        <source>%</source>
        <translation>%</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="179"/>
        <source>RGB</source>
        <translation>Hệ màu RGB</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="204"/>
        <source>G</source>
        <translation>G</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="231"/>
        <source>B</source>
        <translation>B</translation>
    </message>
    <message>
        <location filename="../app/ui/colorinspector.ui" line="258"/>
        <source>R</source>
        <translation>R</translation>
    </message>
    <message>
        <location filename="../app/src/colorinspector.cpp" line="33"/>
        <source>Color Inspector</source>
        <comment>Window title of color inspector</comment>
        <translation>Bộ kiểm tra Màu sắc</translation>
    </message>
</context>
<context>
    <name>ColorPalette</name>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="20"/>
        <source>Color Palette</source>
        <comment>Window title of color palette.</comment>
        <translation>Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="53"/>
        <source>Add Color</source>
        <translation>Thêm màu</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="88"/>
        <source>Remove Color</source>
        <translation>Xóa màu</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="148"/>
        <source>Native color dialog window</source>
        <translation>Cửa sổ hộp thoại Màu hệ thống</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="279"/>
        <source>List Mode</source>
        <translation>Chế độ Danh sách</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="282"/>
        <source>Show palette as a list</source>
        <translation>Hiển thị bảng màu theo dạng danh sách</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="290"/>
        <source>Grid Mode</source>
        <translation>Chế độ Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="293"/>
        <source>Show palette as icons</source>
        <translation>Hiển thị bảng màu theo dạng biểu tượng</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="303"/>
        <source>Small swatch</source>
        <translation>Swatch cỡ nhỏ</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="306"/>
        <source>Sets swatch size to: 16x16px</source>
        <translation>Chỉnh kích cỡ swatch thành: 16x16px</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="314"/>
        <source>Medium Swatch</source>
        <translation>Swatch cỡ trung</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="317"/>
        <source>Sets swatch size to: 26x26px</source>
        <translation>Chỉnh kích cỡ swatch thành: 26x26px</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="328"/>
        <source>Large Swatch</source>
        <translation>Swatch cỡ lớn</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="331"/>
        <source>Sets swatch size to: 36x36px</source>
        <translation>Chỉnh kích cỡ swatch thành: 36x36px</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="339"/>
        <source>Fit Swatch</source>
        <translation>Swatch cỡ vừa vặn</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="342"/>
        <source>Fit swatch to window (19-36 px)</source>
        <translation>Chỉnh kích cỡ swatch vừa với cửa sổ (19-36px)</translation>
    </message>
    <message>
        <location filename="../app/ui/colorpalette.ui" line="345"/>
        <source>Swatch fits window</source>
        <translation>Swatch phù hợp kích cỡ cửa sổ</translation>
    </message>
</context>
<context>
    <name>ColorPaletteWidget</name>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="126"/>
        <source>Add</source>
        <translation>Thêm</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="127"/>
        <source>Replace</source>
        <translation>Thay thế</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="128"/>
        <source>Remove</source>
        <translation>Xóa bỏ</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="266"/>
        <location filename="../app/src/colorpalettewidget.cpp" line="267"/>
        <source>Color name</source>
        <translation>Tên màu</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="613"/>
        <source>The color(s) you are about to delete are currently being used by one or multiple strokes.</source>
        <translation>(Các) Màu mà bạn định xóa đang được sử dụng bởi một hoặc nhiều nét.</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="614"/>
        <source>Cancel</source>
        <translation>Hủy</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="615"/>
        <source>Delete</source>
        <translation>Xóa</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="631"/>
        <source>Palette Restriction</source>
        <translation>Hạn chế của Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/colorpalettewidget.cpp" line="632"/>
        <source>The palette requires at least one swatch to remain functional</source>
        <translation>Bảng màu cần ít nhất một swatch để hoạt động</translation>
    </message>
</context>
<context>
    <name>ColorRef</name>
    <message>
        <location filename="../core_lib/src/graphics/vector/colorref.cpp" line="28"/>
        <source>Green</source>
        <translation>Green</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="284"/>
        <source>Vivid Pink</source>
        <translation>Hồng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="285"/>
        <source>Strong Pink</source>
        <translation>Hồng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="286"/>
        <source>Deep Pink</source>
        <translation>Hồng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="287"/>
        <source>Light Pink</source>
        <translation>Hồng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="288"/>
        <source>Moderate Pink</source>
        <translation>Hồng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="289"/>
        <source>Dark Pink</source>
        <translation>Hồng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="290"/>
        <source>Pale Pink</source>
        <translation>Hồng nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="291"/>
        <source>Grayish Pink</source>
        <translation>Hồng xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="292"/>
        <source>Pinkish White</source>
        <translation>Trắng hồng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="293"/>
        <source>Pinkish Gray</source>
        <translation>Xám hồng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="294"/>
        <source>Vivid Red</source>
        <translation>Đỏ rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="295"/>
        <source>Strong Red</source>
        <translation>Đỏ mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="296"/>
        <source>Deep Red</source>
        <translation>Đỏ đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="297"/>
        <source>Very Deep Red</source>
        <translation>Very Deep Red</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="298"/>
        <source>Moderate Red</source>
        <translation>Đỏ vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="299"/>
        <source>Dark Red</source>
        <translation>Đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="300"/>
        <source>Very Dark Red</source>
        <translation>Đỏ cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="301"/>
        <source>Light Grayish Red</source>
        <translation>Đỏ xám sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="302"/>
        <source>Grayish Red</source>
        <translation>Đỏ xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="303"/>
        <source>Dark Grayish Red</source>
        <translation>Đỏ xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="304"/>
        <source>Blackish Red</source>
        <translation>Đỏ đen</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="305"/>
        <source>Reddish Gray</source>
        <translation>Xám đỏ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="306"/>
        <source>Dark Reddish Gray</source>
        <translation>Xám đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="307"/>
        <source>Reddish Black</source>
        <translation>Đen đỏ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="308"/>
        <source>Vivid Yellowish Pink</source>
        <translation>Hồng vàng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="309"/>
        <source>Strong Yellowish Pink</source>
        <translation>Hồng vàng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="310"/>
        <source>Deep Yellowish Pink</source>
        <translation>Hồng vàng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="311"/>
        <source>Light Yellowish Pink</source>
        <translation>Hồng vàng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="312"/>
        <source>Moderate Yellowish Pink</source>
        <translation>Hồng vàng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="313"/>
        <source>Dark Yellowish Pink</source>
        <translation>Hồng vàng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="314"/>
        <source>Pale Yellowish Pink</source>
        <translation>Hồng vàng nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="315"/>
        <source>Grayish Yellowish Pink</source>
        <translation>Hồng vàng xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="316"/>
        <source>Brownish Pink</source>
        <translation>Hồng nâu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="317"/>
        <source>Vivid Reddish Orange</source>
        <translation>Cam đỏ rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="318"/>
        <source>Strong Reddish Orange</source>
        <translation>Cam đỏ mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="319"/>
        <source>Deep Reddish Orange</source>
        <translation>Cam đỏ đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="320"/>
        <source>Moderate Reddish Orange</source>
        <translation>Cam đỏ vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="321"/>
        <source>Dark Reddish Orange</source>
        <translation>Cam đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="322"/>
        <source>Grayish Reddish Orange</source>
        <translation>Cam đỏ xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="323"/>
        <source>Strong Reddish Brown</source>
        <translation>Nâu đỏ mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="324"/>
        <source>Deep Reddish Brown</source>
        <translation>Nâu đỏ đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="325"/>
        <source>Light Reddish Brown</source>
        <translation>Nâu đỏ nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="326"/>
        <source>Moderate Reddish Brown</source>
        <translation>Nâu đỏ vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="327"/>
        <source>Dark Reddish Brown</source>
        <translation>Nâu đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="328"/>
        <source>Light Grayish Reddish Brown</source>
        <translation>Nâu đỏ xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="329"/>
        <source>Grayish Reddish Brown</source>
        <translation>Nâu đỏ xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="330"/>
        <source>Dark Grayish Reddish Brown</source>
        <translation>Nâu đỏ xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="331"/>
        <source>Vivid Orange</source>
        <translation>Cam rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="332"/>
        <source>Brilliant Orange</source>
        <translation>Cam đỏ rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="333"/>
        <source>Strong Orange</source>
        <translation>Cam mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="334"/>
        <source>Deep Orange</source>
        <translation>Cam đậm </translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="335"/>
        <source>Light Orange</source>
        <translation>Cam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="336"/>
        <source>Moderate Orange</source>
        <translation>Cam vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="337"/>
        <source>Brownish Orange</source>
        <translation>Cam nâu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="338"/>
        <source>Strong Brown</source>
        <translation>Nâu mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="339"/>
        <source>Deep Brown</source>
        <translation>Nâu đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="340"/>
        <source>Light Brown</source>
        <translation>Nâu nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="341"/>
        <source>Moderate Brown</source>
        <translation>Nâu vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="342"/>
        <source>Dark Brown</source>
        <translation>Nâu tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="343"/>
        <source>Light Grayish Brown</source>
        <translation>Nâu xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="344"/>
        <source>Grayish Brown</source>
        <translation>Nâu xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="345"/>
        <source>Dark Grayish Brown</source>
        <translation>Nâu xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="346"/>
        <source>Light Brownish Gray</source>
        <translation>Xám nâu nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="347"/>
        <source>Brownish Gray</source>
        <translation>Xám nâu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="348"/>
        <source>Brownish Black</source>
        <translation>Đen nâu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="349"/>
        <source>Vivid Orange Yellow</source>
        <translation>Vàng cam rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="350"/>
        <source>Brilliant Orange Yellow</source>
        <translation>Vàng cam sáng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="351"/>
        <source>Strong Orange Yellow</source>
        <translation>Vàng cam mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="352"/>
        <source>Deep Orange Yellow</source>
        <translation>Vàng cam đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="353"/>
        <source>Light Orange Yellow</source>
        <translation>Vàng cam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="354"/>
        <source>Moderate Orange Yellow</source>
        <translation>Vàng cam vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="355"/>
        <source>Dark Orange Yellow</source>
        <translation>Vàng cam tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="356"/>
        <source>Pale Orange Yellow</source>
        <translation>Vàng cam nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="357"/>
        <source>Strong Yellowish Brown</source>
        <translation>Nâu vàng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="358"/>
        <source>Deep Yellowish Brown</source>
        <translation>Nâu vàng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="359"/>
        <source>Light Yellowish Brown</source>
        <translation>Nâu vàng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="360"/>
        <source>Moderate Yellowish Brown</source>
        <translation>Nâu vàng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="361"/>
        <source>Dark Yellowish Brown</source>
        <translation>Nâu vàng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="362"/>
        <source>Light Grayish Yellowish Brown</source>
        <translation>Nâu vàng xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="363"/>
        <source>Grayish Yellowish Brown</source>
        <translation>Nâu vàng xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="364"/>
        <source>Dark Grayish Yellowish Brown</source>
        <translation>Nâu vàng xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="365"/>
        <source>Vivid Yellow</source>
        <translation>Vàng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="366"/>
        <source>Brilliant Yellow</source>
        <translation>Vàng đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="367"/>
        <source>Strong Yellow</source>
        <translation>Vàng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="368"/>
        <source>Deep Yellow</source>
        <translation>Vàng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="369"/>
        <source>Light Yellow</source>
        <translation>Vàng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="370"/>
        <source>Moderate Yellow</source>
        <translation>Vàng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="371"/>
        <source>Dark Yellow</source>
        <translation>Vàng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="372"/>
        <source>Pale Yellow</source>
        <translation>Vàng nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="373"/>
        <source>Grayish Yellow</source>
        <translation>Vàng xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="374"/>
        <source>Dark Grayish Yellow</source>
        <translation>Vàng xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="375"/>
        <source>Yellowish White</source>
        <translation>Trắng vàng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="376"/>
        <source>Yellowish Gray</source>
        <translation>Xám vàng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="377"/>
        <source>Light Olive Brown</source>
        <translation>Nâu ô liu nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="378"/>
        <source>Moderate Olive Brown</source>
        <translation>Nâu ô liu vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="379"/>
        <source>Dark Olive Brown</source>
        <translation>Nâu ô liu tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="380"/>
        <source>Vivid Greenish Yellow</source>
        <translation>Vàng lục rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="381"/>
        <source>Brilliant Greenish Yellow</source>
        <translation>Vàng lục đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="382"/>
        <source>Strong Greenish Yellow</source>
        <translation>Vàng lục mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="383"/>
        <source>Deep Greenish Yellow</source>
        <translation>Vàng lục đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="384"/>
        <source>Light Greenish Yellow</source>
        <translation>Vàng lục nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="385"/>
        <source>Moderate Greenish Yellow</source>
        <translation>Vàng lục vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="386"/>
        <source>Dark Greenish Yellow</source>
        <translation>Vàng lục tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="387"/>
        <source>Pale Greenish Yellow</source>
        <translation>Vàng lục nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="388"/>
        <source>Grayish Greenish Yellow</source>
        <translation>Vàng lục xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="389"/>
        <source>Light Olive</source>
        <translation>Ô liu nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="390"/>
        <source>Moderate Olive</source>
        <translation>Ô liu vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="391"/>
        <source>Dark Olive</source>
        <translation>Ô liu tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="392"/>
        <source>Light Grayish Olive</source>
        <translation>Ô liu xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="393"/>
        <source>Grayish Olive</source>
        <translation>Ô liu xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="394"/>
        <source>Dark Grayish Olive</source>
        <translation>Ô liu xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="395"/>
        <source>Light Olive Gray</source>
        <translation>Xám ô liu nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="396"/>
        <source>Olive Gray</source>
        <translation>Xám ô liu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="397"/>
        <source>Olive Black</source>
        <translation>Đem ô liu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="398"/>
        <source>Vivid Yellow Green</source>
        <translation>Lục vàng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="399"/>
        <source>Brilliant Yellow Green</source>
        <translation>Lục vàng đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="400"/>
        <source>Strong Yellow Green</source>
        <translation>Lục vàng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="401"/>
        <source>Deep Yellow Green</source>
        <translation>Lục vàng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="402"/>
        <source>Light Yellow Green</source>
        <translation>Lục vàng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="403"/>
        <source>Moderate Yellow Green</source>
        <translation>Lục vàng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="404"/>
        <source>Pale Yellow Green</source>
        <translation>Lục vàng nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="405"/>
        <source>Grayish Yellow Green</source>
        <translation>Lục vàng xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="406"/>
        <source>Strong Olive Green</source>
        <translation>Lục ô liu mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="407"/>
        <source>Deep Olive Green</source>
        <translation>Lục ô liu đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="408"/>
        <source>Moderate Olive Green</source>
        <translation>Lục ô liu vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="409"/>
        <source>Dark Olive Green</source>
        <translation>Lục ô liu tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="410"/>
        <source>Grayish Olive Green</source>
        <translation>Lục ô liu xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="411"/>
        <source>Dark Grayish Olive Green</source>
        <translation>Lục ô liu xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="412"/>
        <source>Vivid Yellowish Green</source>
        <translation>Lục vàng rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="413"/>
        <source>Brilliant Yellowish Green</source>
        <translation>Lục vàng đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="414"/>
        <source>Strong Yellowish Green</source>
        <translation>Lục vàng mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="415"/>
        <source>Deep Yellowish Green</source>
        <translation>Lục vàng đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="416"/>
        <source>Very Deep Yellowish Green</source>
        <translation>Lục vàng cực đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="417"/>
        <source>Very Light Yellowish Green</source>
        <translation>Very Light Yellowish Green</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="418"/>
        <source>Light Yellowish Green</source>
        <translation>Lục vàng nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="419"/>
        <source>Moderate Yellowish Green</source>
        <translation>Lục vàng vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="420"/>
        <source>Dark Yellowish Green</source>
        <translation>Lục vàng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="421"/>
        <source>Very Dark Yellowish Green</source>
        <translation>Lục vàng cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="422"/>
        <source>Vivid Green</source>
        <translation>Lục rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="423"/>
        <source>Brilliant Green</source>
        <translation>Lục đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="424"/>
        <source>Strong Green</source>
        <translation>Lục mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="425"/>
        <source>Deep Green</source>
        <translation>Lục đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="426"/>
        <source>Very Light Green</source>
        <translation>Lục sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="427"/>
        <source>Light Green</source>
        <translation>Lục nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="428"/>
        <source>Moderate Green</source>
        <translation>Lục vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="429"/>
        <source>Dark Green</source>
        <translation>Lục tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="430"/>
        <source>Very Dark Green</source>
        <translation>Lục cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="431"/>
        <source>Very Pale Green</source>
        <translation>Lục cực nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="432"/>
        <source>Pale Green</source>
        <translation>Lục nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="433"/>
        <source>Grayish Green</source>
        <translation>Lục xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="434"/>
        <source>Dark Grayish Green</source>
        <translation>Lục xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="435"/>
        <source>Blackish Green</source>
        <translation>Lục đen</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="436"/>
        <source>Greenish White</source>
        <translation>Trắng lục</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="437"/>
        <source>Light Greenish Gray</source>
        <translation>Xám lục nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="438"/>
        <source>Greenish Gray</source>
        <translation>Xám lục</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="439"/>
        <source>Dark Greenish Gray</source>
        <translation>Xám lục tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="440"/>
        <source>Greenish Black</source>
        <translation>Đen lục</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="441"/>
        <source>Vivid Bluish Green</source>
        <translation>Lục lam rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="442"/>
        <source>Brilliant Bluish Green</source>
        <translation>Lục lam đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="443"/>
        <source>Strong Bluish Green</source>
        <translation>Lục lam mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="444"/>
        <source>Deep Bluish Green</source>
        <translation>Lục lam đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="445"/>
        <source>Very Light Bluish Green</source>
        <translation>Lục lam sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="446"/>
        <source>Light Bluish Green</source>
        <translation>Lục lam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="447"/>
        <source>Moderate Bluish Green</source>
        <translation>Lục lam vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="448"/>
        <source>Dark Bluish Green</source>
        <translation>Lục lam tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="449"/>
        <source>Very Dark Bluish Green</source>
        <translation>Lục lam cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="450"/>
        <source>Vivid Greenish Blue</source>
        <translation>Lam lục rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="451"/>
        <source>Brilliant Greenish Blue</source>
        <translation>Lam lục đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="452"/>
        <source>Strong Greenish Blue</source>
        <translation>Lam lục mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="453"/>
        <source>Deep Greenish Blue</source>
        <translation>Lam lục đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="454"/>
        <source>Very Light Greenish Blue</source>
        <translation>Lam lục sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="455"/>
        <source>Light Greenish Blue</source>
        <translation>Lam lục nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="456"/>
        <source>Moderate Greenish Blue</source>
        <translation>Lam lục vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="457"/>
        <source>Dark Greenish Blue</source>
        <translation>Lam lục tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="458"/>
        <source>Very Dark Greenish Blue</source>
        <translation>Lam lục cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="459"/>
        <source>Vivid Blue</source>
        <translation>Lam rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="460"/>
        <source>Brilliant Blue</source>
        <translation>Lam đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="461"/>
        <source>Strong Blue</source>
        <translation>Lam mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="462"/>
        <source>Deep Blue</source>
        <translation>Lam đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="463"/>
        <source>Very Light Blue</source>
        <translation>Lam sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="464"/>
        <source>Light Blue</source>
        <translation>Lam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="465"/>
        <source>Moderate Blue</source>
        <translation>Lam vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="466"/>
        <source>Dark Blue</source>
        <translation>Lam tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="467"/>
        <source>Very Pale Blue</source>
        <translation>Lam cực nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="468"/>
        <source>Pale Blue</source>
        <translation>Lam nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="469"/>
        <source>Grayish Blue</source>
        <translation>Lam xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="470"/>
        <source>Dark Grayish Blue</source>
        <translation>Lam xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="471"/>
        <source>Blackish Blue</source>
        <translation>Lam đen</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="472"/>
        <source>Bluish White</source>
        <translation>Trắng lam</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="473"/>
        <source>Light Bluish Gray</source>
        <translation>Xám lam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="474"/>
        <source>Bluish Gray</source>
        <translation>Xám lam </translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="475"/>
        <source>Dark Bluish Gray</source>
        <translation>Xám lam tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="476"/>
        <source>Bluish Black</source>
        <translation>Đen lam</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="477"/>
        <source>Vivid Purplish Blue</source>
        <translation>Lam tím rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="478"/>
        <source>Brilliant Purplish Blue</source>
        <translation>Lam tím đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="479"/>
        <source>Strong Purplish Blue</source>
        <translation>Lam tím mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="480"/>
        <source>Deep Purplish Blue</source>
        <translation>Lam tím đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="481"/>
        <source>Very Light Purplish Blue</source>
        <translation>Lam tím sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="482"/>
        <source>Light Purplish Blue</source>
        <translation>Lam tím nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="483"/>
        <source>Moderate Purplish Blue</source>
        <translation>Lam tím vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="484"/>
        <source>Dark Purplish Blue</source>
        <translation>Lam tím tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="485"/>
        <source>Very Pale Purplish Blue</source>
        <translation>Lam tím sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="486"/>
        <source>Pale Purplish Blue</source>
        <translation>Lam tím nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="487"/>
        <source>Grayish Purplish Blue</source>
        <translation>Lam tím xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="488"/>
        <source>Vivid Violet</source>
        <translation>Tím rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="489"/>
        <source>Brilliant Violet</source>
        <translation>Tím đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="490"/>
        <source>Strong Violet</source>
        <translation>Tím mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="491"/>
        <source>Deep Violet</source>
        <translation>Tím đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="492"/>
        <source>Very Light Violet</source>
        <translation>Tím sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="493"/>
        <source>Light Violet</source>
        <translation>Tím nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="494"/>
        <source>Moderate Violet</source>
        <translation>Tím vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="495"/>
        <source>Dark Violet</source>
        <translation>Tím tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="496"/>
        <source>Very Pale Violet</source>
        <translation>Tím cực nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="497"/>
        <source>Pale Violet</source>
        <translation>Tím nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="498"/>
        <source>Grayish Violet</source>
        <translation>Tím xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="499"/>
        <source>Vivid Purple</source>
        <translation>Tía rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="500"/>
        <source>Brilliant Purple</source>
        <translation>Tía đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="501"/>
        <source>Strong Purple</source>
        <translation>Tía mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="502"/>
        <source>Deep Purple</source>
        <translation>Tía đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="503"/>
        <source>Very Deep Purple</source>
        <translation>Tía cực đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="504"/>
        <source>Very Light Purple</source>
        <translation>Tía sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="505"/>
        <source>Light Purple</source>
        <translation>Tía nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="506"/>
        <source>Moderate Purple</source>
        <translation>Tía vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="507"/>
        <source>Dark Purple</source>
        <translation>Tía tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="508"/>
        <source>Very Dark Purple</source>
        <translation>Tía cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="509"/>
        <source>Very Pale Purple</source>
        <translation>Tía cực nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="510"/>
        <source>Pale Purple</source>
        <translation>Tía nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="511"/>
        <source>Grayish Purple</source>
        <translation>Tía xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="512"/>
        <source>Dark Grayish Purple</source>
        <translation>Tía xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="513"/>
        <source>Blackish Purple</source>
        <translation>Tía đen</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="514"/>
        <source>Purplish White</source>
        <translation>Trắng tía</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="515"/>
        <source>Light Purplish Gray</source>
        <translation>Xám tía nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="516"/>
        <source>Purplish Gray</source>
        <translation>Xám tía </translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="517"/>
        <source>Dark Purplish Gray</source>
        <translation>Xám tía tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="518"/>
        <source>Purplish Black</source>
        <translation>Đen tía</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="519"/>
        <source>Vivid Reddish Purple</source>
        <translation>Tía đỏ rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="520"/>
        <source>Strong Reddish Purple</source>
        <translation>Tía đỏ mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="521"/>
        <source>Deep Reddish Purple</source>
        <translation>Tía đỏ đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="522"/>
        <source>Very Deep Reddish Purple</source>
        <translation>Tía đỏ cực đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="523"/>
        <source>Light Reddish Purple</source>
        <translation>Tía đỏ nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="524"/>
        <source>Moderate Reddish Purple</source>
        <translation>Tía đỏ vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="525"/>
        <source>Dark Reddish Purple</source>
        <translation>Tía đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="526"/>
        <source>Very Dark Reddish Purple</source>
        <translation>Tía đỏ cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="527"/>
        <source>Pale Reddish Purple</source>
        <translation>Tía đỏ nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="528"/>
        <source>Grayish Reddish Purple</source>
        <translation>Tía đỏ xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="529"/>
        <source>Brilliant Purplish Pink</source>
        <translation>Hồng tía đậm rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="530"/>
        <source>Strong Purplish Pink</source>
        <translation>Hồng tía mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="531"/>
        <source>Deep Purplish Pink</source>
        <translation>Hồng tía đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="532"/>
        <source>Light Purplish Pink</source>
        <translation>Hồng tía nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="533"/>
        <source>Moderate Purplish Pink</source>
        <translation>Hồng tía vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="534"/>
        <source>Dark Purplish Pink</source>
        <translation>Hồng tía tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="535"/>
        <source>Pale Purplish Pink</source>
        <translation>Hồng tía nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="536"/>
        <source>Grayish Purplish Pink</source>
        <translation>Hồng tía xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="537"/>
        <source>Vivid Purplish Red</source>
        <translation>Đỏ tía rực rỡ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="538"/>
        <source>Strong Purplish Red</source>
        <translation>Đỏ tía mạnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="539"/>
        <source>Deep Purplish Red</source>
        <translation>Đỏ tía đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="540"/>
        <source>Very Deep Purplish Red</source>
        <translation>Đỏ tía cực đậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="541"/>
        <source>Moderate Purplish Red</source>
        <translation>Đỏ tía vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="542"/>
        <source>Dark Purplish Red</source>
        <translation>Đỏ tía tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="543"/>
        <source>Very Dark Purplish Red</source>
        <translation>Đỏ tía cực tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="544"/>
        <source>Light Grayish Purplish Red</source>
        <translation>Đỏ tía xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="545"/>
        <source>Grayish Purplish Red</source>
        <translation>Đỏ tía xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="546"/>
        <source>White</source>
        <translation>Trắng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="547"/>
        <source>Light Gray</source>
        <translation>Xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="548"/>
        <source>Medium Gray</source>
        <translation>Xám vừa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="549"/>
        <source>Dark Gray</source>
        <translation>Xám tối </translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/colordictionary.h" line="550"/>
        <source>Black</source>
        <translation>Đen</translation>
    </message>
</context>
<context>
    <name>ColorWheel</name>
    <message>
        <location filename="../app/src/colorwheel.cpp" line="30"/>
        <source>Color Wheel</source>
        <comment>Color Wheel&apos;s window title</comment>
        <translation>Bánh xe Màu</translation>
    </message>
</context>
<context>
    <name>ColorizeOptionsWidget</name>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="54"/>
        <source>Colorize Mask</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="62"/>
        <source>Refresh</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="63"/>
        <source>Update All</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="64"/>
        <source>Regenerate coloring for the current frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="65"/>
        <source>Regenerate coloring for every frame of this layer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="71"/>
        <source>Edit key strokes</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="72"/>
        <source>Show output</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="81"/>
        <source>Key Strokes</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="90"/>
        <source>Transparent</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="91"/>
        <source>Remove</source>
        <translation type="unfinished">Xóa bỏ</translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="92"/>
        <source>Mark the selected color as transparent: its stroke areas stay unfilled (use for background)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="93"/>
        <source>Erase all strokes of the selected color on this frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="100"/>
        <source>Edge detection (soft pencil lines)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="107"/>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="113"/>
        <source> px</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="132"/>
        <source>Edge size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="133"/>
        <source>Gap closing radius</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="134"/>
        <source>Cleanup strength</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="137"/>
        <source>Paint color strokes with the brush; mark background color as transparent; press Refresh to fill.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="214"/>
        <source>Line art source: %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="219"/>
        <source>No line art layer found! Add a bitmap layer with drawings.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/colorizeoptionswidget.cpp" line="262"/>
        <source>Transparent (stays unfilled)</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>CommandLineExporter</name>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="59"/>
        <source>Error: No input file specified. An input project file argument is required when output path(s) are specified.</source>
        <translation>Lỗi: Không xác định được tệp nhập. Tham số tệp dự án nhập phải được quy định khi (các) đường dẫn xuất được chỉ định.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="78"/>
        <source>Warning: the specified camera layer %1 was not found, ignoring.</source>
        <translation>Cảnh báo: không tìm thấy layer máy quay %1 được chỉ định, bỏ qua.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="110"/>
        <source>Warning: Output format is not specified or unsupported. Using PNG.</source>
        <comment>Command line warning</comment>
        <translation>Cảnh báo: Định dạng dữ liệu xuất không được chỉ định hoặc không được hỗ trợ. Đang sử dụng PNG.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="135"/>
        <source>Warning: Transparency is not currently supported in movie files</source>
        <comment>Command line warning</comment>
        <translation>Cảnh báo: Độ trong suốt trong các tệp phim hiện không được hỗ trợ</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="138"/>
        <source>Exporting movie...</source>
        <comment>Command line task progress</comment>
        <translation>Đang xuất phim...</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="150"/>
        <location filename="../app/src/commandlineexporter.cpp" line="174"/>
        <source>Done.</source>
        <comment>Command line task done</comment>
        <translation>Hoàn tất.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineexporter.cpp" line="161"/>
        <source>Exporting image sequence...</source>
        <comment>Command line task progress</comment>
        <translation>Đang xuất chuỗi hình ảnh...</translation>
    </message>
</context>
<context>
    <name>CommandLineParser</name>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="30"/>
        <source>Pencil2D is an animation/drawing software for Mac OS X, Windows, and Linux. It lets you create traditional hand-drawn animation (cartoon) using both bitmap and vector graphics.</source>
        <translation>Pencil2D là một phần mềm hoạt hình / vẽ cho Mac OS X, Windows và Linux. Nó cho phép bạn tạo hoạt ảnh vẽ tay truyền thống (phim hoạt hình) bằng cách sử dụng cả đồ họa bitmap và đồ họa vector.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="34"/>
        <source>Path to the input pencil file.</source>
        <translation>Đường dẫn đến tệp pencil đầu vào.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="37"/>
        <location filename="../app/src/commandlineparser.cpp" line="43"/>
        <source>Render the file to &lt;output_path&gt;</source>
        <translation>Kết xuất tệp tới vị trí&lt;output_path&gt;</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="38"/>
        <location filename="../app/src/commandlineparser.cpp" line="44"/>
        <source>output_path</source>
        <translation>output_path</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="51"/>
        <source>Name of the camera layer to use</source>
        <translation>Đặt tên layer máy quay</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="52"/>
        <source>layer_name</source>
        <translation>layer_name</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="56"/>
        <source>Width of the output frames</source>
        <translation>Chiều rộng của các khung hình đầu ra</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="57"/>
        <location filename="../app/src/commandlineparser.cpp" line="62"/>
        <source>integer</source>
        <translation>số nguyên</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="61"/>
        <source>Height of the output frames</source>
        <translation>Chiều cao của khung hình đầu ra</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="66"/>
        <source>The first frame you want to include in the exported movie</source>
        <translation>Khung hình đầu tiên trong phim được xuất ra</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="67"/>
        <location filename="../app/src/commandlineparser.cpp" line="74"/>
        <source>frame</source>
        <translation>khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="71"/>
        <source>The last frame you want to include in the exported movie. Can also be last or last-sound to automatically use the last frame containing animation or sound, respectively</source>
        <translation>Frame hình ảnh cuối cùng bạn mong muốn thêm vào phim được xuất. Có thể là cuối hoặc âm thanh cuối tự động sử dụng frame hình ảnh cuối cùng chứa hoạt ảnh hoặc âm thanh, lần lượt</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="78"/>
        <source>Render transparency when possible</source>
        <translation>Kết xuất độ trong suốt nếu có thể</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="105"/>
        <source>Warning: width value %1 is not an integer, ignoring.</source>
        <translation>Cảnh báo: giá trị chiều ngang %1 không phải là số nguyên, bỏ qua.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="116"/>
        <source>Warning: height value %1 is not an integer, ignoring.</source>
        <translation>Cảnh báo: giá trị chiều cao%1 không phải là số nguyên, bỏ qua.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="127"/>
        <source>Warning: start value %1 is not an integer, ignoring.</source>
        <translation>Cảnh báo: giá trị khởi điểm %1 không phải là số nguyên, bỏ qua.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="132"/>
        <source>Warning: start value must be at least 1, ignoring.</source>
        <translation>Cảnh báo: giá trị khởi điểm phải lớn hơn hoặc bằng 1, bỏ qua.</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="153"/>
        <source>Warning: end value %1 is not an integer, last or last-sound, ignoring.</source>
        <translation>Cảnh báo: giả trị cuối %1 không phải là số nguyên, cuối hoặc âm thanh cuối, bỏ qua</translation>
    </message>
    <message>
        <location filename="../app/src/commandlineparser.cpp" line="159"/>
        <source>Warning: end value %1 is smaller than start value %2, ignoring.</source>
        <translation>Cảnh báo: giá trị kết thúc %1 nhỏ hơn giá trị bắt đầu %2, bỏ qua.</translation>
    </message>
</context>
<context>
    <name>DoubleProgressDialog</name>
    <message>
        <location filename="../app/ui/doubleprogressdialog.ui" line="27"/>
        <source>Loading...</source>
        <translation>Đang tải...</translation>
    </message>
    <message>
        <location filename="../app/ui/doubleprogressdialog.ui" line="56"/>
        <source>Cancel</source>
        <translation>Huỷ</translation>
    </message>
</context>
<context>
    <name>Editor</name>
    <message>
        <source>Copy</source>
        <translation type="vanished">Sao chép</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="220"/>
        <source>Cut frames</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="255"/>
        <source>Paste from Previous Keyframe</source>
        <translation>Dán từ khung hình chính trước</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="327"/>
        <source>Paste frames</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="349"/>
        <source>Paste</source>
        <translation>Dán</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="362"/>
        <source>Flip selection vertically</source>
        <translation>Lật vùng chọn theo chiều dọc</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="362"/>
        <source>Flip selection horizontally</source>
        <translation>Lật vùng chọn theo chiều ngang</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="376"/>
        <source>Reposition frame</source>
        <translation>Định vị lại khung hình</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="464"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="473"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="482"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="514"/>
        <source>Could not open file</source>
        <translation>Không thể mở tệp</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="465"/>
        <source>The file you have selected is a directory, so we are unable to open it. If you are are trying to open a project that uses the old structure, please open the file ending with .pcl, not the data folder.</source>
        <translation>Bạn vừa chọn một thư mục, chúng tôi không thể mở nó. Nếu bạn đang muốn mở một đồ án sử dụng cấu trúc cũ, vui lòng mở tệp có định dạng .pcl, không phải là thư mục dữ liệu.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="474"/>
        <source>The file you have selected does not exist, so we are unable to open it. Please make sure that you&apos;ve entered the correct path and that the file is accessible and try again.</source>
        <translation>Tập tin mà bạn vừa chọn không tồn tại, chúng tôi không thể mở nó. Vui lòng đảm bảo rằng bạn đã nhập đúng đường dẫn và tập tin có thể được truy cập rồi thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="483"/>
        <source>This program does not have permission to read the file you have selected. Please check that you have read permissions for this file and try again.</source>
        <translation>Chương trình này không có quyền đọc tập tin mà bạn đã chọn. Vui lòng kiểm tra rằng bạn đã ủy quyền hạn đọc tập tin này rồi thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="515"/>
        <source>An unknown error occurred while trying to load the file and we are not able to load your file.</source>
        <translation>Đã xảy ra lỗi không xác định trong quá trình tải và chúng tôi không thể tải tập tin của bạn.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="604"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="717"/>
        <source>File not found at path &quot;%1&quot;. Please check the image is present at the specified location and try again.</source>
        <translation>Tập tin không được tìm thấy tại đường dẫn &quot;%1&quot;. Xin hãy đảm bảo hình ảnh tồn tại tại vị trí được chỉ định và thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="607"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="720"/>
        <source>Image format is not supported. Please convert the image file to one of the following formats and try again:
%1</source>
        <translation>Định dạng hình ảnh không được hỗ trợ. Xin hãy thay đổi tập tin hình ảnh sang một trong những định dạng dưới đây và thử lại:
%1</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="611"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="724"/>
        <source>An error has occurred while reading the image. Please check that the file is a valid image and try again.</source>
        <translation>Đã xảy ra lỗi khi đang xử lý hình ảnh. Xin hãy đảm bảo tập tin là một hình ảnh hợp lệ và thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="614"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="678"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="693"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="700"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="727"/>
        <source>Import failed</source>
        <translation>Nhập không thành công</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="630"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="739"/>
        <source>Import Image</source>
        <translation>Nhập vào hình ảnh</translation>
    </message>
    <message>
        <source>You cannot import images into a vector layer.</source>
        <translation type="vanished">Bạn không thể nhập hình ảnh vào vector layer.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="678"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="693"/>
        <source>You can only import images to a bitmap layer.</source>
        <translation>Bạn chỉ có thể nhập hoặc chuyển hình ảnh vào bitmap layer.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="700"/>
        <source>The selected image has a format that does not support animation.</source>
        <translation>Hình ảnh được chọn có định dạng không hỗ trợ hoạt ảnh.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="887"/>
        <source>Add frame</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/editor.cpp" line="916"/>
        <location filename="../core_lib/src/interface/editor.cpp" line="923"/>
        <source>Remove frame</source>
        <translation>Xóa khung hình</translation>
    </message>
</context>
<context>
    <name>ErrorDialog</name>
    <message>
        <location filename="../app/ui/errordialog.ui" line="20"/>
        <source>Dialog</source>
        <translation>Hộp hội thoại</translation>
    </message>
    <message>
        <location filename="../app/ui/errordialog.ui" line="55"/>
        <source>&lt;h3&gt;Title&lt;/h3&gt;</source>
        <translation>&lt;h3&gt;Tiêu Đề&lt;/h3&gt;</translation>
    </message>
    <message>
        <location filename="../app/ui/errordialog.ui" line="68"/>
        <source>Description</source>
        <translation>Mô tả</translation>
    </message>
    <message>
        <location filename="../app/ui/errordialog.ui" line="88"/>
        <source>This report contains vital information. Copy all of it when submitting a bug.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/errordialog.cpp" line="41"/>
        <source>Copy to Clipboard</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ExportImageDialog</name>
    <message>
        <location filename="../app/src/exportimagedialog.cpp" line="29"/>
        <source>Export image sequence</source>
        <translation>Xuất chuỗi hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/exportimagedialog.cpp" line="33"/>
        <source>Export image</source>
        <translation>Xuất hình ảnh</translation>
    </message>
</context>
<context>
    <name>ExportImageOptions</name>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="23"/>
        <source>Camera</source>
        <translation>Máy quay</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="35"/>
        <source>Resolution</source>
        <translation>Độ phân giải</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="76"/>
        <source>Format</source>
        <translation>Định dạng</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="83"/>
        <source>PNG</source>
        <translation>Ảnh hỗ trợ nền trong suốt PNG</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="88"/>
        <source>JPG</source>
        <translation>Ảnh thường JPG</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="93"/>
        <source>BMP</source>
        <translation>Ảnh dạng Bitmap BMP</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="98"/>
        <source>TIFF</source>
        <translation>Ảnh TIFF</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="103"/>
        <source>WEBP</source>
        <translation>Ảnh hỗ trợ nền trong suốt WEBP</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="111"/>
        <source>Transparency</source>
        <translation>Trong suốt</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="121"/>
        <source>Range</source>
        <translation>Khoảng vùng</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="154"/>
        <source>The last frame you want to include in the exported movie</source>
        <translation>Khung hình cuối cùng mà bạn muốn có trong phim được xuất</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="157"/>
        <source>End Frame</source>
        <translation>Khung hình cuối</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="182"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;End frame is set to last paintable keyframe (Useful when you only want to export to the last animated frame)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Khung hình cuối được thiết lập thành khung hình chính cuối cùng có thể vẽ (Hữu ích khi bạn chỉ muốn xuất tới khung hình cuối)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt; </translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="188"/>
        <source>To the end of sound clips</source>
        <translation>Đến cuối clip âm thanh</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="220"/>
        <source>The first frame you want to include in the exported movie</source>
        <translation>Khung hình đầu tiên mà bạn muốn có trong phim xuất ra</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="223"/>
        <source>Start Frame</source>
        <translation>Khung hình đầu</translation>
    </message>
    <message>
        <location filename="../app/ui/exportimageoptions.ui" line="243"/>
        <source>Export keyframes only</source>
        <translation>Chỉ xuất các khung hình chính</translation>
    </message>
</context>
<context>
    <name>ExportMovieDialog</name>
    <message>
        <location filename="../app/src/exportmoviedialog.cpp" line="29"/>
        <source>Export Animated GIF</source>
        <translation>Xuất ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/exportmoviedialog.cpp" line="32"/>
        <source>Export Movie</source>
        <translation>Xuất phim</translation>
    </message>
</context>
<context>
    <name>ExportMovieOptions</name>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="29"/>
        <source>Camera</source>
        <translation>Máy quay</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="41"/>
        <source>Resolution</source>
        <translation>Độ phân giải</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="59"/>
        <source>Width</source>
        <translation>Chiều rộng</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="66"/>
        <source>The MP4 format does not support odd width. Please specify an even width or use a different file format.</source>
        <translation>Định dạng MP4 không hỗ trợ độ phân giải chiều ngang là một số lẻ. Hãy chỉ định một số chẵn hoặc dùng một định dạng khác.</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="98"/>
        <source>Height</source>
        <translation>Chiều cao</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="105"/>
        <source>The MP4 format does not support odd height. Please specify an even height or use a different file format.</source>
        <translation>Định dạng MP4 không hỗ trợ độ phân giải chiều dọc là một số lẻ. Hãy chỉ định một số chẵn hoặc dùng một định dạng khác.</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="128"/>
        <source>Range</source>
        <translation>Khoảng vùng</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="174"/>
        <source>The last frame you want to include in the exported movie</source>
        <translation>Khung hình cuối cùng mà bạn muốn xuất trong phim</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="177"/>
        <source>End Frame</source>
        <translation>Khung hình cuối</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="196"/>
        <source>The first frame you want to include in the exported movie</source>
        <translation>Khung hình đầu mà bạn muốn xuất ra trong phim</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="199"/>
        <source>Start Frame</source>
        <translation>Khung hình đầu</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="224"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;End frame is set to last paintable keyframe (Useful when you only want to export to the last animated frame)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;Khung hình cuối được thiết lập thành khung hình chính cuối cùng có thể vẽ (Hữu ích khi bạn chỉ muốn xuất tới khung hình chính cuối)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt; </translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="230"/>
        <source>To the end of sound clips</source>
        <translation>Đến cuối của clip âm thanh</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="253"/>
        <source>GIF and APNG only</source>
        <translation>Chỉ tệp GIF và APNG</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="256"/>
        <source>Loop</source>
        <translation>Vòng lặp</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="269"/>
        <source>Exporter Settings</source>
        <translation>Cài đặt Xuất dữ liệu</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="287"/>
        <source>WebM and APNG only</source>
        <translation>Chỉ tệp WebM và APNG</translation>
    </message>
    <message>
        <location filename="../app/ui/exportmovieoptions.ui" line="290"/>
        <source>Transparency</source>
        <translation>Độ trong suốt</translation>
    </message>
</context>
<context>
    <name>FileDialog</name>
    <message>
        <location filename="../app/src/filedialog.cpp" line="167"/>
        <source>Open animation</source>
        <translation>Mở hoạt hình</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="168"/>
        <source>Import image</source>
        <translation>Nhập vào hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="169"/>
        <source>Import image sequence</source>
        <translation>Nhập vào chuỗi hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="170"/>
        <source>Import Animated GIF</source>
        <translation>Nhập ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="171"/>
        <source>Import animated image</source>
        <translation>Nhập ảnh động</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="172"/>
        <source>Import movie</source>
        <translation>Nhập vào phim</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="173"/>
        <source>Import sound</source>
        <translation>Nhập vào âm thanh</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="174"/>
        <source>Open palette</source>
        <translation>Mở bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="183"/>
        <source>Save animation</source>
        <translation>Lưu hoạt hình</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="184"/>
        <source>Export image</source>
        <translation>Xuất ra hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="185"/>
        <source>Export image sequence</source>
        <translation>Xuất ra chuỗi hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="186"/>
        <source>Export Animated GIF</source>
        <translation>Xuất ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="187"/>
        <source>Export animated image</source>
        <translation>Xuất ảnh động</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="188"/>
        <source>Export movie</source>
        <translation>Xuất ra phim</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="190"/>
        <source>Export palette</source>
        <translation>Xuất ra bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="218"/>
        <source>Animated GIF</source>
        <translation>Ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="275"/>
        <source>untitled</source>
        <translation>chưa có tiêu đề</translation>
    </message>
    <message>
        <location filename="../app/src/filedialog.cpp" line="282"/>
        <source>MyAnimation</source>
        <translation>Hoạt ảnh của tôi</translation>
    </message>
</context>
<context>
    <name>FileFormat</name>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="32"/>
        <source>Pencil2D formats</source>
        <translation>Các định dạng của Pencil2D</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="32"/>
        <location filename="../core_lib/src/util/fileformat.h" line="35"/>
        <source>Pencil2D Project</source>
        <translation>Dự án Pencil2D</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="32"/>
        <location filename="../core_lib/src/util/fileformat.h" line="35"/>
        <source>Legacy Pencil2D Project</source>
        <translation>Dự án cũ Pencil2D</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="38"/>
        <source>Movie formats</source>
        <translation>Định dạng phim</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="43"/>
        <location filename="../core_lib/src/util/fileformat.h" line="46"/>
        <source>Image formats</source>
        <translation>Định dạng hình ảnh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="49"/>
        <source>Palette formats</source>
        <translation>Định dạng Bảng màu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="49"/>
        <source>Pencil2D Palette</source>
        <translation>Bảng màu Pencil2D</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="49"/>
        <source>GIMP Palette</source>
        <translation>Bảng màu GIMP</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="52"/>
        <source>Animated GIF</source>
        <translation>Ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="55"/>
        <source>Animated image formats</source>
        <translation>Định dạng ảnh động</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/fileformat.h" line="58"/>
        <source>Sound formats</source>
        <translation>Định dạng âm thanh</translation>
    </message>
</context>
<context>
    <name>FileManager</name>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="250"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="265"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="273"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="280"/>
        <source>Invalid Save Path</source>
        <translation>Đường dẫn lưu tập tin không hợp lệ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="251"/>
        <source>The path is empty.</source>
        <translation>Đường dẫn rỗng.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="266"/>
        <source>The path (&quot;%1&quot;) points to a directory.</source>
        <translation>Đường dẫn (&quot;%1&quot;) trỏ tới thư mục.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="274"/>
        <source>The directory (&quot;%1&quot;) does not exist.</source>
        <translation>Thư mục (&quot;%1&quot;) không tồn tại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="281"/>
        <source>The path (&quot;%1&quot;) is not writable.</source>
        <translation>Đường dẫn (&quot;%1&quot;) không thể ghi đè.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="319"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="328"/>
        <source>Cannot Create Data Directory</source>
        <translation>Không tạo được thư mục dữ liệu</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="320"/>
        <source>Failed to create directory &quot;%1&quot;. Please make sure you have sufficient permissions.</source>
        <translation>Tạo thư mục &quot;%1&quot; thất bại. Vui lòng đảm bảo bạn có đủ quyền hạn.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="329"/>
        <source>&quot;%1&quot; is a file. Please delete the file and try again.</source>
        <translation>&quot;%1&quot; là một tập tin. Vui lòng xóa tập tin đó và thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="358"/>
        <source>An internal error occurred. The project could not be saved.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="368"/>
        <source>Miniz Error</source>
        <translation>Lỗi mã hóa Miniz</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="357"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="385"/>
        <source>Internal Error</source>
        <translation>Lỗi phát sinh nội tại</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="369"/>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="386"/>
        <source>An internal error occurred. The project may not have been saved successfully.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="554"/>
        <source>Could not open file</source>
        <translation>Không thể mở tập tin</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="564"/>
        <source>The file does not exist, so we are unable to open it.Please check to make sure the path is correct and try again.</source>
        <translation>Tập tin này không tồn tại, chúng tôi không thể mở tập tin. Vui lòng đảm bảo đường dẫn chính xác và thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="569"/>
        <source>No permission to read the file. Please check you have read permissions for this file and try again.</source>
        <translation>Không có quyền đọc tập tin. Vui lòng đảm bảo bạn đã ủy quyền hạn đọc tập tin này rồi thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="575"/>
        <source>There was an error processing your file. This usually means that your project has been at least partially corrupted. Try again with a newer version of Pencil2D, or try to use a backup file if you have one. If you contact us through one of our official channels we may be able to help you.For reporting issues, the best places to reach us are:</source>
        <translation>Đã xảy ra lỗi khi xử lý tập tin của bạn. Điều này thường có nghĩa là dự án của bạn ít hay nhiều đã bị hỏng. Hãy thử lại với phiên bản mới hơn của Pencil2D, hoặc sử dụng tập tin sao lưu nếu bạn có. Nếu bạn liên hệ với chúng tôi thông qua một trong các kênh chính thức, chúng tôi có thể giúp bạn. Để báo cáo lỗi, hãy liên hệ với chúng tôi thông qua:</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="1073"/>
        <source>Bitmap Layer %1</source>
        <translation>Layer Bitmap %1</translation>
    </message>
    <message>
        <source>Vector Layer %1</source>
        <translation type="vanished">Layer Vector %1</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/filemanager.cpp" line="1075"/>
        <source>Sound Layer %1</source>
        <translation>Layer Âm thanh %1</translation>
    </message>
</context>
<context>
    <name>FilesPage</name>
    <message>
        <location filename="../app/ui/filespage.ui" line="17"/>
        <source>Startup Settings</source>
        <translation>Cài đặt Khởi động</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="25"/>
        <location filename="../app/ui/filespage.ui" line="28"/>
        <source>Saving the current project as a preset</source>
        <translation>Lưu dự án hiện tại thành preset</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="31"/>
        <source>+</source>
        <translation>+</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="38"/>
        <source>-</source>
        <translation>-</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="45"/>
        <source>Make Default</source>
        <translation>Cài làm mặc định</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="61"/>
        <source>Ask on startup</source>
        <translation>Hỏi khi khởi động</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="74"/>
        <source>Load default preset</source>
        <translation>Tải preset mặc định</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="87"/>
        <source>Load last active file</source>
        <translation>Tải tập tin đã mở lần trước</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="100"/>
        <source>Autosave documents</source>
        <comment>Preference</comment>
        <translation>Tự động lưu tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="106"/>
        <source>Enable autosave by number of modifications</source>
        <comment>Preference</comment>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="152"/>
        <source>Enable autosave by time</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="177"/>
        <source>Autosave period (minutes)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Enable autosave</source>
        <comment>Preference</comment>
        <translation type="vanished">Kích hoạt chức năng tự lưu tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/filespage.ui" line="113"/>
        <source>Number of modifications before autosaving:</source>
        <comment>Preference</comment>
        <translation>Số lần chỉnh sửa trước khi tự động lưu tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/filespage.cpp" line="98"/>
        <source>&lt;br&gt;&lt;br&gt;Error: your preset may not have saved successfully. If you believe that this error is an issue with Pencil2D, please create a new issue at:&lt;br&gt;&lt;a href=&apos;https://github.com/pencil2d/pencil/issues&apos;&gt;https://github.com/pencil2d/pencil/issues&lt;/a&gt;&lt;br&gt;Please include the following details in your issue:</source>
        <translation>&lt;br&gt;&lt;br&gt;Lỗi: preset lưu thất bại. Nếu bạn nghĩ rằng đây là vấn đề phát sinh bởi Pencil2D, hãy ghi lại vấn đề này tại:&lt;br&gt;&lt;a href=&apos;https://github.com/pencil2d/pencil/issues&apos;&gt;https://github.com/pencil2d/pencil/issues&lt;/a&gt;&lt;br&gt;Vui lòng bao gồm các chi tiết sau về vấn đề mà bạn gặp phải:</translation>
    </message>
</context>
<context>
    <name>GeneralPage</name>
    <message>
        <location filename="../app/ui/generalpage.ui" line="38"/>
        <source>Language</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Ngôn ngữ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="44"/>
        <location filename="../app/ui/generalpage.ui" line="48"/>
        <source>[System-Language]</source>
        <comment>First item of the language list</comment>
        <translation>[Ngôn ngữ hệ thống]</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="59"/>
        <source>Window opacity</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Độ mờ cửa sổ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="65"/>
        <source>Opacity</source>
        <translation>Độ trong suốt</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="88"/>
        <source>Appearance</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Giao diện</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="94"/>
        <source>Shadows</source>
        <translation>Đổ bóng</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="101"/>
        <source>Tool Cursors</source>
        <translation>Công cụ con trỏ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="108"/>
        <source>Canvas Cursor</source>
        <translation>Con trỏ trên vùng vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="118"/>
        <source>Background</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Hình nền</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="162"/>
        <source>Canvas</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Vùng vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="168"/>
        <source>Antialiasing</source>
        <translation>Khử răng cưa</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="191"/>
        <source>Editing</source>
        <comment>GroupBox title in Preference</comment>
        <translation>Hiệu chỉnh</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="197"/>
        <source>Vector curve smoothing</source>
        <translation>Độ mượt khi vẽ vector</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="217"/>
        <source>Tablet high-resolution position</source>
        <translation>Vị trí Độ phân giải cao dành cho bảng vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="227"/>
        <source>Grid</source>
        <comment>groupBox title in Preference</comment>
        <translation>Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="233"/>
        <source>Grid Height</source>
        <translation>Độ cao Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="240"/>
        <source>Enable Grid</source>
        <translation>Kích hoạt Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="285"/>
        <source>Grid Width</source>
        <translation>Độ rộng Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="295"/>
        <source>Overlays</source>
        <translation>Lớp phủ</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="311"/>
        <source>Enable Action Safe area (%)</source>
        <translation>Kích hoạt vùng An toàn cho Nội dung (%)</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="354"/>
        <source>Enable Title Safe area (%)</source>
        <translation>Kích hoạt vùng An toàn cho Tiêu đề (%)</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="394"/>
        <source>Show Safe area labels</source>
        <translation>Hiển thị các nhãn của vùng An toàn</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="404"/>
        <source>Scroll Wheel Zoom</source>
        <translation>Thu-Phóng bằng cuộn chuột</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="422"/>
        <source>Invert Scroll Direction</source>
        <translation>Đảo ngược hướng cuộn</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="432"/>
        <source>Advanced</source>
        <comment>groupBox title in Preference</comment>
        <translation>Nâng cao</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="449"/>
        <source>Memory Cache Budget</source>
        <translation>Dung lượng Bộ nhớ đệm</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="480"/>
        <source>MB</source>
        <translation>MB</translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="501"/>
        <source>Undo/Redo</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="507"/>
        <source>Enable New System (Experimental)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="519"/>
        <source>How many steps you&apos;re allowed to undo/redo</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="522"/>
        <source>Maximum Number of Undo/Redo Steps</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="555"/>
        <source>Apply</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/generalpage.ui" line="562"/>
        <source>Cancel</source>
        <translation>Huỷ</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="43"/>
        <source>Arabic</source>
        <translation>Tiếng Ả Rập</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="44"/>
        <source>Bulgarian</source>
        <translation>Tiếng Bulgaria</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="45"/>
        <source>Catalan</source>
        <translation>Tiếng Catalonia</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="46"/>
        <source>Czech</source>
        <translation>Tiếng Séc</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="47"/>
        <source>Danish</source>
        <translation>Tiếng Đan Mạch</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="48"/>
        <source>German</source>
        <translation>Tiếng Đức</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="49"/>
        <source>Greek</source>
        <translation>Tiếng Hy Lạp</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="50"/>
        <source>English</source>
        <translation>Tiếng Anh</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="51"/>
        <source>Spanish</source>
        <translation>Tiếng Tây Ban Nha</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="52"/>
        <source>Estonian</source>
        <translation>Tiếng Estonia</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="53"/>
        <source>Persian</source>
        <translation>Tiếng Ba Tư</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="54"/>
        <source>French</source>
        <translation>Tiếng Pháp</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="55"/>
        <source>Hebrew</source>
        <translation>Tiếng Do Thái</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="56"/>
        <source>Hungarian</source>
        <translation>Tiếng Hungary</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="57"/>
        <source>Indonesian</source>
        <translation>Tiếng Indonesia</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="58"/>
        <source>Italian</source>
        <translation>Tiếng Ý</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="59"/>
        <source>Japanese</source>
        <translation>Tiếng Nhật</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="60"/>
        <source>Kabyle</source>
        <translation>Tiếng Kabyle</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="61"/>
        <source>Korean</source>
        <translation>Tiếng Hàn Quốc</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="62"/>
        <source>Norwegian Bokmål</source>
        <translation>Tiếng Na Uy - chữ Bokmål</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="63"/>
        <source>Dutch – Netherlands</source>
        <translation>Tiếng Hà Lan</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="64"/>
        <source>Polish</source>
        <translation>Tiếng Ba Lan</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="65"/>
        <source>Portuguese – Portugal</source>
        <translation>Tiếng Bồ Đào Nha - Bồ Đào Nha</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="66"/>
        <source>Portuguese – Brazil</source>
        <translation>Tiếng Bồ Đào Nha - Brazil</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="67"/>
        <source>Russian</source>
        <translation>Tiếng Nga</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="68"/>
        <source>Slovene</source>
        <translation>Tiếng Slovene</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="69"/>
        <source>Swedish</source>
        <translation>Tiếng Thụy Điển</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="70"/>
        <source>Turkish</source>
        <translation>Tiếng Thổ Nhĩ Kỳ</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="71"/>
        <source>Vietnamese</source>
        <translation>Tiếng Việt</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="72"/>
        <source>Cantonese</source>
        <translation>Tiếng Quảng Đông</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="73"/>
        <source>Chinese – China</source>
        <translation>Tiếng Trung – Trung Quốc</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="74"/>
        <source>Chinese – Taiwan</source>
        <translation>Tiếng Trung – Đài Loan</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="229"/>
        <source>Restart Required</source>
        <translation>Yêu cầu khởi Động lại</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="230"/>
        <source>The language change will take effect after a restart of Pencil2D</source>
        <translation>Thay đổi ngôn ngữ sẽ Được thực hiện sau khi khởi động lại phần mềm Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="369"/>
        <source>Resets your current undo history</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="370"/>
        <source>Changing the maximum number of undo/redo steps resets your current undo/redo history. 

Are you sure you want to proceed?</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="385"/>
        <source>Experimental feature!</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="386"/>
        <source>This feature is work in progress and may not currently allow for the same features as the current undo/redo system. Once enabled, you&apos;ll need to restart the application to start using it. 

Do you still want to try?</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/generalpage.cpp" line="398"/>
        <source>The undo/redo system will be changed on the next launch of the application</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>ImportExportDialog</name>
    <message>
        <location filename="../app/ui/importexportdialog.ui" line="38"/>
        <source>Instructions</source>
        <translation>Hướng dẫn</translation>
    </message>
    <message>
        <location filename="../app/ui/importexportdialog.ui" line="48"/>
        <source>File</source>
        <translation>Tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/importexportdialog.ui" line="79"/>
        <source>Browse...</source>
        <translation>Duyệt tập tin...</translation>
    </message>
    <message>
        <location filename="../app/ui/importexportdialog.ui" line="89"/>
        <source>Options</source>
        <translation>Lựa chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/importexportdialog.ui" line="96"/>
        <source>Imports</source>
        <translation>Nhập liệu</translation>
    </message>
</context>
<context>
    <name>ImportImageSeqDialog</name>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="64"/>
        <source>Import Animated GIF</source>
        <translation>Nhập ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="67"/>
        <source>Import image sequence</source>
        <translation>Nhập chuỗi hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="70"/>
        <source>Import animated image</source>
        <translation>Nhập ảnh động</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="79"/>
        <source>Import predefined keyframe set</source>
        <translation>Nhập bộ nhóm khung hình chính định trước</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="80"/>
        <source>Select an image that matches the criteria: MyFile000.png, eg. Joe001.png 
The importer will search and find images matching the same criteria. You can see the result in the preview box below.</source>
        <translation>Chọn một ảnh phù hợp với tiêu chuẩn: MyFile000.png, eg. Joe001.png
Phần nhập liệu sẽ tìm kiếm hình ảnh phù hợp với tiêu chuẩn. Bạn có thể thấy kết quả ở hộp xem trước ở dưới.</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="179"/>
        <source>Importing image sequence...</source>
        <translation>Đang nhập chuỗi hình ảnh...</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="179"/>
        <location filename="../app/src/importimageseqdialog.cpp" line="293"/>
        <source>Abort</source>
        <translation>Hủy</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="293"/>
        <source>Importing images...</source>
        <translation>Đang nhập các hình ảnh...</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="352"/>
        <location filename="../app/src/importimageseqdialog.cpp" line="377"/>
        <source>Invalid path</source>
        <translation>Đường dẫn không hợp lệ</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="353"/>
        <source>The following file did not meet the criteria: 
%1 

Read the instructions and try again</source>
        <translation>Tập tin sau đây không đáp ứng tiêu chí:
%1

Đọc hướng dẫn và thử lại</translation>
    </message>
    <message>
        <location filename="../app/src/importimageseqdialog.cpp" line="378"/>
        <source>The following file(-s) did not meet the criteria: 
%1</source>
        <translation>(Các) file sau đây không đáp ứng tiêu chí: 
%1</translation>
    </message>
</context>
<context>
    <name>ImportImageSeqOptions</name>
    <message>
        <location filename="../app/ui/importimageseqoptions.ui" line="38"/>
        <source>Import an image every # frame</source>
        <translation>Nhập vào một ảnh mỗi # khung hình</translation>
    </message>
</context>
<context>
    <name>ImportImageSeqPreviewGroupBox</name>
    <message>
        <location filename="../app/ui/importimageseqpreview.ui" line="14"/>
        <source>GroupBox</source>
        <translation>GroupBox</translation>
    </message>
</context>
<context>
    <name>ImportLayersDialog</name>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="14"/>
        <source>Import Layers from other *.pclx files</source>
        <translation>Nhập các layer từ các file *.pclx</translation>
    </message>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="22"/>
        <source>1. Select Project file:</source>
        <translation>1. Chọn một tập tin dự án:</translation>
    </message>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="42"/>
        <source>Select File</source>
        <translation>Chọn tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="51"/>
        <source>2. Select layers from file:</source>
        <translation>2. Chọn các layer từ tập tin:</translation>
    </message>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="76"/>
        <source>Close</source>
        <translation>Đóng</translation>
    </message>
    <message>
        <location filename="../app/ui/importlayersdialog.ui" line="83"/>
        <source>Import layers</source>
        <translation>Nhập các layer</translation>
    </message>
    <message>
        <location filename="../app/src/importlayersdialog.cpp" line="62"/>
        <source>Choose file</source>
        <translation>Chọn tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/importlayersdialog.cpp" line="119"/>
        <source>Opening document...</source>
        <translation>Đang mở tài liệu...</translation>
    </message>
    <message>
        <location filename="../app/src/importlayersdialog.cpp" line="119"/>
        <source>Abort</source>
        <translation>Hủy</translation>
    </message>
</context>
<context>
    <name>ImportPositionDialog</name>
    <message>
        <location filename="../app/ui/importpositiondialog.ui" line="14"/>
        <source>Import position</source>
        <translation>Vị trí nhập</translation>
    </message>
    <message>
        <location filename="../app/ui/importpositiondialog.ui" line="22"/>
        <source>Import image/s relative to:</source>
        <translation>Nhập ảnh vào vị trí tương đối với: </translation>
    </message>
    <message>
        <location filename="../app/src/importpositiondialog.cpp" line="31"/>
        <source>Center of current view</source>
        <translation>Tâm của chế độ xem hiện tại</translation>
    </message>
    <message>
        <location filename="../app/src/importpositiondialog.cpp" line="32"/>
        <source>Center of canvas (0,0)</source>
        <translation>Tâm vùng vẽ (0,0)</translation>
    </message>
    <message>
        <location filename="../app/src/importpositiondialog.cpp" line="33"/>
        <source>Center of camera, current frame</source>
        <translation>Tâm máy quay, khung hình hiện tại</translation>
    </message>
    <message>
        <location filename="../app/src/importpositiondialog.cpp" line="34"/>
        <source>Center of camera, follow camera</source>
        <translation>Tâm máy quay, di chuyển theo máy quay</translation>
    </message>
</context>
<context>
    <name>Layer</name>
    <message>
        <location filename="../core_lib/src/structure/layer.cpp" line="39"/>
        <source>Undefined Layer</source>
        <translation>Layer chưa xác định</translation>
    </message>
</context>
<context>
    <name>LayerBitmap</name>
    <message>
        <location filename="../core_lib/src/structure/layerbitmap.cpp" line="28"/>
        <source>Bitmap Layer</source>
        <translation>Bitmap Layer</translation>
    </message>
</context>
<context>
    <name>LayerCamera</name>
    <message>
        <location filename="../core_lib/src/structure/layercamera.cpp" line="27"/>
        <source>Camera Layer</source>
        <translation>Layer máy quay</translation>
    </message>
</context>
<context>
    <name>LayerColorize</name>
    <message>
        <location filename="../core_lib/src/structure/layercolorize.cpp" line="28"/>
        <source>Colorize Layer</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>LayerOpacityDialog</name>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="14"/>
        <source>Layer / Keyframe Opacity</source>
        <translation>Độ mờ Layer / Khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="22"/>
        <source>Layer: </source>
        <translation>Layer:</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="46"/>
        <location filename="../app/ui/layeropacitydialog.ui" line="65"/>
        <source>% transparency</source>
        <translation>% trong suốt</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="68"/>
        <source> %</source>
        <translation>%</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="83"/>
        <source>Set opacity for:</source>
        <translation>Hiệu chỉnh độ mờ của:</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="89"/>
        <source>Active keyframe</source>
        <translation>Khung hình chính hiện hành</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="99"/>
        <source>Selected keyframe(s)</source>
        <translation>(Các) khung hình chính được chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="106"/>
        <source>Layer</source>
        <translation>Layer</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="116"/>
        <source>Fade in / Fade out</source>
        <translation>Fade in / Fade out</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="122"/>
        <source>Fade in over selcted keyframes</source>
        <translation>Hiểu ứng Fade in đối với các khung hình chính được chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="125"/>
        <source>Fade in</source>
        <translation>Fade in</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="135"/>
        <source>Fade out over selected keyframes</source>
        <translation>Hiệu ứng fade out đối với các khung hình chính được chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="138"/>
        <source>Fade out</source>
        <translation>Fade out</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="166"/>
        <source>Close</source>
        <translation>Đóng</translation>
    </message>
    <message>
        <location filename="../app/ui/layeropacitydialog.ui" line="178"/>
        <source>Be aware that opacity changes are made in the rendering, and will not change your artwork.</source>
        <translation>Lưu ý rằng các thay đổi về độ mờ được thực hiện trong kết xuất và sẽ không thay đổi tác phẩm của bạn.</translation>
    </message>
    <message>
        <location filename="../app/src/layeropacitydialog.cpp" line="59"/>
        <source>Layer: %1</source>
        <translation>Layer: %1</translation>
    </message>
</context>
<context>
    <name>LayerSound</name>
    <message>
        <location filename="../core_lib/src/structure/layersound.cpp" line="29"/>
        <source>Sound Layer</source>
        <translation>Layer âm thanh</translation>
    </message>
</context>
<context>
    <name>LayerVector</name>
    <message>
        <source>Vector Layer</source>
        <translation type="vanished">Vector layer</translation>
    </message>
</context>
<context>
    <name>LipsyncDialog</name>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="92"/>
        <source>口型同步切换器</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="97"/>
        <source>口型图层命名 A/E/I/O/U/N/MBP/FV/L/WQ，点击即在“口型”图层的当前帧插入该口型。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="105"/>
        <source>刷新口型</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="106"/>
        <source>清空此帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="119"/>
        <location filename="../app/src/tvptoolsdialog.cpp" line="122"/>
        <source>口型</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="153"/>
        <source>在当前帧插入口型 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="184"/>
        <source>未找到口型图层（图层名为 A/E/I/O/U 等）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="211"/>
        <source>插入口型 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="233"/>
        <source>清空口型帧</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>MainWindow2</name>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="14"/>
        <source>MainWindow</source>
        <translation>Cửa sổ chính</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="75"/>
        <source>File</source>
        <translation>Tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="79"/>
        <source>Import</source>
        <translation>Nhập vào</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="97"/>
        <source>Export</source>
        <translation>Xuất ra</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="119"/>
        <source>Edit</source>
        <translation>Chỉnh sửa</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="123"/>
        <source>Selection</source>
        <translation>Chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="155"/>
        <source>View</source>
        <translation>Hiển thị</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="159"/>
        <source>Onion Skin</source>
        <translation>Onion skin</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="166"/>
        <source>Zoom</source>
        <translation>Thu phóng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="182"/>
        <source>Layer Visibility</source>
        <translation>Độ hiển thị Layer</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="190"/>
        <source>Overlays</source>
        <translation>Lớp phủ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="194"/>
        <source>Perspective Lines Angle</source>
        <translation>Góc đường phối cảnh</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="240"/>
        <source>Animation</source>
        <translation>Hoạt hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="244"/>
        <source>Timeline Selection</source>
        <translation>Chọn Dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="276"/>
        <source>Tools</source>
        <translation>Công cụ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="297"/>
        <source>Layer</source>
        <translation>Layer</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="301"/>
        <source>Change line color</source>
        <translation>Thay đổi màu của nét</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="318"/>
        <location filename="../app/ui/mainwindow2.ui" line="848"/>
        <source>Help</source>
        <translation>Trợ giúp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="326"/>
        <source>Windows</source>
        <translation>Cửa sổ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="330"/>
        <source>Toolbars</source>
        <translation>Thanh công cụ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="354"/>
        <source>New</source>
        <translation>Tạo Mới</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="363"/>
        <source>Open</source>
        <translation>Mở ra</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="372"/>
        <source>Save</source>
        <translation>Lưu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="381"/>
        <source>Save As...</source>
        <translation>Lưu dưới dạng...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="386"/>
        <source>Exit</source>
        <translation>Thoát</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="391"/>
        <location filename="../app/ui/mainwindow2.ui" line="419"/>
        <source>Image Sequence...</source>
        <translation>Chuỗi hình ảnh...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="396"/>
        <location filename="../app/ui/mainwindow2.ui" line="414"/>
        <source>Image...</source>
        <translation>Hình ảnh...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="401"/>
        <source>Movie...</source>
        <translation>Phim...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="409"/>
        <source>Palette</source>
        <translation>Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="424"/>
        <source>Movie Video...</source>
        <translation>Video phim...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="429"/>
        <source>Sound...</source>
        <translation>Âm thanh...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="434"/>
        <source>Image Predefined set...</source>
        <translation>Bộ hình ảnh định trước</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="443"/>
        <source>Undo</source>
        <translation>Hoàn tác</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="452"/>
        <source>Redo</source>
        <translation>Thực hiện lại</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="464"/>
        <source>Cut</source>
        <translation>Cắt</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="476"/>
        <source>Copy</source>
        <translation>Sao chép</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="488"/>
        <source>Paste</source>
        <translation>Dán</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="699"/>
        <source>Onion Align</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="717"/>
        <source>Lasso</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="726"/>
        <source>Deform</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1093"/>
        <source>Center</source>
        <comment>To move sth. to the center</comment>
        <translation>Trung tâm</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1103"/>
        <source>Replace Paper with Transparency</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1139"/>
        <location filename="../app/ui/mainwindow2.ui" line="1142"/>
        <source>Paste from Previous Keyframe</source>
        <translation>Dán từ khung hình chính trước</translation>
    </message>
    <message>
        <source>Show Invisible Lines</source>
        <translation type="vanished">Hiển thị những đường nét ẩn</translation>
    </message>
    <message>
        <source>Show Outlines Only</source>
        <translation type="vanished">Chỉ hiển thị đường viền</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1154"/>
        <source>Center</source>
        <comment>The middle point of an area</comment>
        <translation>Trung tâm</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1166"/>
        <source>Thirds</source>
        <translation>Một phần ba</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1178"/>
        <source>Golden Ratio</source>
        <translation>Tỉ lệ vàng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1190"/>
        <source>Safe Areas</source>
        <translation>Khu vực an toàn</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1202"/>
        <source>One Point Perspective</source>
        <translation>Góc nhìn tại một điểm</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1214"/>
        <source>Two Point Perspective</source>
        <translation>Góc nhìn tại hai điểm</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1226"/>
        <source>Three Point Perspective</source>
        <translation>Góc nhìn tại ba điểm</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1234"/>
        <source>2°</source>
        <translation>Góc hoặc hướng: 2 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1242"/>
        <source>3°</source>
        <translation>Góc hoặc hướng: 3 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1250"/>
        <source>5°</source>
        <translation>Góc hoặc hướng: 5 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1258"/>
        <source>7.5°</source>
        <translation>Góc hoặc hướng: 7.5 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1266"/>
        <source>10°</source>
        <translation>Góc hoặc hướng: 10 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1274"/>
        <source>15°</source>
        <translation>Góc hoặc hướng: 15 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1282"/>
        <source>20°</source>
        <translation>Góc hoặc hướng: 20 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1290"/>
        <source>30°</source>
        <translation>Góc hoặc hướng: 30 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="493"/>
        <source>Select All</source>
        <translation>Chọn tất cả</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="130"/>
        <source>Prepare Scanned Drawings</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="498"/>
        <source>Deselect All</source>
        <translation>Bỏ chọn tất cả</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="507"/>
        <source>Clear Frame</source>
        <translation>Làm sạch khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="512"/>
        <source>Preferences</source>
        <translation>Cài Đặt chung</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="520"/>
        <source>Reset Windows</source>
        <translation>Đặt lại cửa sổ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="529"/>
        <source>Zoom In</source>
        <translation>Phóng to</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="538"/>
        <source>Zoom Out</source>
        <translation>Thu nhỏ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="543"/>
        <source>Rotate Clockwise</source>
        <translation>Xoay theo chiều kim đồng hồ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="548"/>
        <source>Rotate Anticlockwise</source>
        <translation>Xoay ngược chiều kim đồng hồ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="557"/>
        <source>Reset</source>
        <translation>Đặt lại</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="569"/>
        <source>Horizontal Flip</source>
        <translation>Lật theo chiều ngang</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="581"/>
        <source>Vertical Flip</source>
        <translation>Lật theo chiều dọc</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="593"/>
        <source>Grid</source>
        <translation>Lưới</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="605"/>
        <source>Previous</source>
        <translation>Trước Đó</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="608"/>
        <source>Show previous onion skin</source>
        <translation>HIển thị Onion Skin trước đó</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="620"/>
        <source>Next</source>
        <translation>Kế tiếp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="623"/>
        <source>Show next onion skin</source>
        <translation>Hiển thị Onion skin kế tiếp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="632"/>
        <location filename="../app/src/mainwindow2.cpp" line="1856"/>
        <source>Play</source>
        <translation>Chạy</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="644"/>
        <source>Loop</source>
        <translation>Lặp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="649"/>
        <source>Next Frame</source>
        <translation>Khung hình kế tiếp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="654"/>
        <source>Previous Frame</source>
        <translation>Khung hình trước đó</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="663"/>
        <source>Add Frame</source>
        <translation>Thêm khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="672"/>
        <source>Duplicate Frame</source>
        <translation>Tạo bản sao khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="681"/>
        <source>Remove Frame</source>
        <translation>Gỡ bỏ khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="690"/>
        <source>Move</source>
        <translation>Di chuyển</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="708"/>
        <source>Select</source>
        <translation>Chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="735"/>
        <source>Brush</source>
        <translation>Cọ vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="744"/>
        <source>Polyline</source>
        <translation>Polyline</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="753"/>
        <source>Smudge</source>
        <translation>Làm mờ</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="762"/>
        <source>Pen</source>
        <translation>Viết mực</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="771"/>
        <source>Hand</source>
        <translation>Bàn tay</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="780"/>
        <source>Pencil</source>
        <translation>Bút chì</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="789"/>
        <source>Bucket</source>
        <translation>Xô màu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="798"/>
        <source>Eyedropper</source>
        <translation>Chọn màu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="807"/>
        <source>Eraser</source>
        <translation>Tẩy xóa</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="816"/>
        <source>New Bitmap Layer</source>
        <translation>Layer Bitmap mới</translation>
    </message>
    <message>
        <source>New Vector Layer</source>
        <translation type="vanished">Layer Vector mới</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="825"/>
        <source>New Sound Layer</source>
        <translation>Layer âm thanh mới</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="834"/>
        <source>New Camera Layer</source>
        <translation>Layer máy quay mới</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="843"/>
        <source>Delete Current Layer</source>
        <translation>Xóa layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="853"/>
        <source>About</source>
        <translation>Giới thiệu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="858"/>
        <location filename="../app/ui/mainwindow2.ui" line="861"/>
        <source>Reset to default</source>
        <translation>Đưa về mặc định</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="871"/>
        <location filename="../app/ui/mainwindow2.ui" line="874"/>
        <source>Next Keyframe</source>
        <translation>Khung hình chính kế tiếp</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="884"/>
        <location filename="../app/ui/mainwindow2.ui" line="887"/>
        <source>Previous KeyFrame</source>
        <translation>Khung hình chính trước đó</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="898"/>
        <source>Range</source>
        <translation>Khoảng vùng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="903"/>
        <source>Flip X</source>
        <translation>Lật theo trục X</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="908"/>
        <source>Flip Y</source>
        <translation>Lật theo trục Y</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="913"/>
        <source>Move Frame Forward</source>
        <translation>Di chuyển khung hình tới trước</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="918"/>
        <source>Move Frame Backward</source>
        <translation>Di chuyển khung hình về sau</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="923"/>
        <source>Pencil2D Website</source>
        <translation>Trang web Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="928"/>
        <source>Report a Bug</source>
        <translation>Báo cáo Lỗi</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="933"/>
        <source>Quick Reference Guide</source>
        <translation>Tài liệu Tham khảo Nhanh</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="936"/>
        <source>F1</source>
        <translation>F1</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="941"/>
        <source>Animated Image...</source>
        <translation>Ảnh động...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="946"/>
        <source>Animated GIF...</source>
        <translation>Ảnh động GIF...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="951"/>
        <source>Check for Updates</source>
        <translation>Kiểm tra bản cập nhật</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="956"/>
        <source>Pencil2D Forum</source>
        <translation>Diễn đàn Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="961"/>
        <source>Pencil2D Discord</source>
        <translation>Discord Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="966"/>
        <source>200%</source>
        <translation>200%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="971"/>
        <source>300%</source>
        <translation>300%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="976"/>
        <source>400%</source>
        <translation>400%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="981"/>
        <source>50%</source>
        <translation>50%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="986"/>
        <source>33%</source>
        <translation>33%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="991"/>
        <source>25%</source>
        <translation>25%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="996"/>
        <source>100%</source>
        <translation>100%</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1001"/>
        <source>Flip In-Between</source>
        <translation>Lật trang In-Between</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1006"/>
        <source>Flip Rolling</source>
        <translation>Lật trang Xoay vòng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1011"/>
        <source>Peg Bar Alignment</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1049"/>
        <source>Current layer only</source>
        <translation>Chỉ layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1057"/>
        <source>Relative</source>
        <translation>Tương đối</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1016"/>
        <source>Movie Audio...</source>
        <translation>Âm thanh Phim...</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1021"/>
        <source>Append to Palette...</source>
        <translation>Nối tới Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1026"/>
        <source>Replace Palette...</source>
        <translation>Thay thế Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1031"/>
        <source>Current keyframe</source>
        <translation>Khung hình chính hiện hành</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1036"/>
        <source>All keyframes on layer</source>
        <translation>Mọi khung hình chính trên layer</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1041"/>
        <source>Layers from Project file...</source>
        <translation>Các layer từ tập tin dự án</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1065"/>
        <source>All layers</source>
        <translation>Mọi layer</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1070"/>
        <source>Reposition Selected Frames</source>
        <translation>Định vị lại các khung hình đã chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1075"/>
        <source>Layer / Keyframe opacity</source>
        <translation>Độ mờ Layer / Khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1080"/>
        <source>Open Temporary Directory</source>
        <translation>Mở Thư mục Tạm thời</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1088"/>
        <source>Lock Windows</source>
        <translation>Khóa cửa sổ hiển thị</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1098"/>
        <source>Reset Rotation</source>
        <translation>Đưa về Góc xoay Mặc định</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1108"/>
        <source>Add Exposure</source>
        <translation>Tăng độ Phơi sáng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1113"/>
        <source>Subtract Exposure</source>
        <translation>Giảm độ Phơi sáng</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1118"/>
        <source>Reverse Frames Order</source>
        <translation>Đảo ngược Thứ tự Khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1123"/>
        <source>Remove Frames</source>
        <translation>Xóa Khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/mainwindow2.ui" line="1134"/>
        <source>Status Bar</source>
        <translation>Thanh Trạng thái</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="192"/>
        <source>color palette:&lt;br&gt;use &lt;b&gt;(C)&lt;/b&gt;&lt;br&gt;toggle at cursor</source>
        <translation>&lt;b&gt;Bảng màu:&lt;br&gt;Sử dụng (C)&lt;/b&gt;&lt;br&gt;bật tắt con trỏ</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="196"/>
        <source>Color inspector</source>
        <translation>Kiểm tra màu</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="525"/>
        <source>Open Recent</source>
        <translation>Mở gần đây</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="532"/>
        <source>工作区</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="571"/>
        <location filename="../app/src/mainwindow2.cpp" line="595"/>
        <source>Dialog is already open!</source>
        <translation>Hộp thoại đã được mở!</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="645"/>
        <source>Please select at least 2 frames!</source>
        <translation>Xin hãy chọn ít nhất 2 khung hình!</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="769"/>
        <source>Opening document...</source>
        <translation>Đang mở tài liệu...</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="769"/>
        <location filename="../app/src/mainwindow2.cpp" line="826"/>
        <source>Abort</source>
        <translation>Hủy</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="814"/>
        <location filename="../app/src/mainwindow2.cpp" line="915"/>
        <source>Warning</source>
        <translation>Cảnh báo</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="815"/>
        <source>This program does not currently have permission to write to the file you have selected. Please make sure you have write permission for this file before attempting to save it. Alternatively, you can use the Save As... menu option to save to a writable location.</source>
        <translation>Chương trình này hiện không có quyền viết lên tập tin bạn đã chọn. Vui lòng đảm bảo bạn có quyền viết lên tập tin này trước khi lưu. Thay vào đó, bạn có thể sử dụng tùy chọn menu Lưu dưới dạng...  để lưu ở vị trí khác.</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="826"/>
        <source>Saving document...</source>
        <translation>Đang lưu tài liệu...</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="916"/>
        <source>This animation has been modified.
 Do you want to save your changes?</source>
        <translation> Hoạt hình này đã thay đổi.
Bạn có muốn lưu lại những thay đổi này?</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="939"/>
        <source>AutoSave Reminder</source>
        <translation>Nhắc nhở Tự động lưu</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="940"/>
        <source>The animation is not saved yet.
 Do you want to save now?</source>
        <translation>Hoạt hình này chưa được lưu.
Bạn có muốn lưu ngay không?</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="941"/>
        <source>Never ask again</source>
        <comment>AutoSave reminder button</comment>
        <translation>Đừng hỏi lại</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1157"/>
        <source>保存工作区</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1158"/>
        <source>工作区名称：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1169"/>
        <source>覆盖工作区</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1170"/>
        <source>工作区“%1”已存在，是否覆盖？</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1196"/>
        <location filename="../app/src/mainwindow2.cpp" line="1261"/>
        <source>删除工作区</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1197"/>
        <source>确定删除工作区“%1”？</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1241"/>
        <source>重置默认布局</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1243"/>
        <source>保存当前工作区…</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1760"/>
        <source>时间轴工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1761"/>
        <source>口型同步切换器</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1762"/>
        <source>调色板提取</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1763"/>
        <source>视频抽帧中割</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="869"/>
        <source>&lt;br&gt;&lt;br&gt;An error has occurred and your file may not have saved successfully.
If you believe that this error is an issue with Pencil2D, please create a new issue at:&lt;br&gt;&lt;a href=&apos;https://github.com/pencil2d/pencil/issues&apos;&gt;https://github.com/pencil2d/pencil/issues&lt;/a&gt;&lt;br&gt;Please be sure to include the following details in your issue:</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Opening a palette will replace the old palette.
Color(s) in strokes will be altered by this action!</source>
        <translation type="vanished">Việc mở bảng màu mới sẽ thay thế bảng màu cũ.
(Các) Màu được sử dụng bới các nét vẽ sẽ bị thay đổi bởi thao tác này!</translation>
    </message>
    <message>
        <source>Open Palette</source>
        <translation type="vanished">Mở Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1851"/>
        <source>Stop</source>
        <translation>Ngừng ngay</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1927"/>
        <source>Restore Project?</source>
        <translation>Phục hồi Dự án?</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1928"/>
        <source>Pencil2D didn&apos;t close correctly. Would you like to restore the project?</source>
        <translation>Pencil2D gặp lỗi khi đang đóng. Bạn có muốn phục hồi dự án?</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1932"/>
        <source>Restore project</source>
        <translation>Phục hồi Dự án</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1965"/>
        <source>Recovery Failed.</source>
        <translation>Khôi phục Thất bại.</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1966"/>
        <source>Sorry! Pencil2D is unable to restore your project</source>
        <translation>Xin lỗi! Pencil2D không thể khôi phục dự án của bạn</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1976"/>
        <source>Recovery Succeeded!</source>
        <translation>Phục hồi Thành công!</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1977"/>
        <source>Please save your work immediately to prevent loss of data</source>
        <translation>Vui lòng lưu lại hoạt động của bạn để tránh việc mất dữ liệu</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="1985"/>
        <source>Main Toolbar</source>
        <translation>Thanh công cụ chính</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="2002"/>
        <source>View Toolbar</source>
        <translation>Hiển thị thanh công cụ</translation>
    </message>
    <message>
        <location filename="../app/src/mainwindow2.cpp" line="2009"/>
        <source>Overlay Toolbar</source>
        <translation>Che lấp thanh công cụ</translation>
    </message>
</context>
<context>
    <name>MovieExporter</name>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="86"/>
        <source>Checking environment...</source>
        <translation>Đang kiểm tra môi trường...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="120"/>
        <source>Generating GIF...</source>
        <translation>Đang tạo GIF...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="127"/>
        <source>Assembling audio...</source>
        <translation>Đang ghép âm thanh...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="132"/>
        <source>Generating movie...</source>
        <translation>Đang tạo phim...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="137"/>
        <source>Done</source>
        <translation>Hoàn tất</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="595"/>
        <location filename="../core_lib/src/movieexporter.cpp" line="607"/>
        <location filename="../core_lib/src/movieexporter.cpp" line="732"/>
        <location filename="../core_lib/src/movieexporter.cpp" line="744"/>
        <source>Something went wrong</source>
        <translation>Đã xảy ra sự cố</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="596"/>
        <location filename="../core_lib/src/movieexporter.cpp" line="733"/>
        <source>Looks like our video backend did not exit normally. Your movie may not have exported correctly. Please try again and report this if it persists.</source>
        <translation>Có vẻ như chương trình phụ trợ video của chúng tôi đã không thoát bình thường. Phim của bạn có thể đã xuất không chính xác. Vui lòng thử lại và báo cáo điều này nếu tình trạng vẫn tiếp diễn.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieexporter.cpp" line="608"/>
        <location filename="../core_lib/src/movieexporter.cpp" line="745"/>
        <source>Couldn&apos;t start the video backend, please try again.</source>
        <translation>Không thể khởi động chương trình phụ trợ video, vui lòng thử lại.</translation>
    </message>
</context>
<context>
    <name>MovieImporter</name>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="54"/>
        <location filename="../core_lib/src/movieimporter.cpp" line="248"/>
        <source>Bitmap only</source>
        <translation>Chỉ bitmap</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="55"/>
        <location filename="../core_lib/src/movieimporter.cpp" line="249"/>
        <source>You need to be on the bitmap layer to import a movie clip</source>
        <translation>Cần ở trên layer bitmap để nhập phim</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="155"/>
        <source>Loading video failed</source>
        <translation>Tải video thất bại</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="156"/>
        <source>Could not get duration from the specified video. Are you sure you are importing a valid video file?</source>
        <translation>Không thể lấy thời lượng từ video đã chỉ định. Bạn có chắc mình đang nhập tập tin video hợp lệ không?</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="181"/>
        <source>Error creating folder</source>
        <translation>Lỗi tạo thư mục</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="182"/>
        <source>Unable to create a temporary folder, cannot import video.</source>
        <translation>Không thể tạo thư mục tạm thời, không thể nhập video.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="196"/>
        <source>Imported movie too big!</source>
        <translation>Dung lượng phim vừa nhập quá lớn!</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="197"/>
        <source>The movie clip is too long. Pencil2D can only hold %1 frames, but this movie would go up to about frame %2. Please make your video shorter and try again.</source>
        <translation>Clip phim quá dài. Pencil2D chỉ có thẻ chứa %1 frame, trong khi đoạn phim này chứa tới %2 frame. Vui lòng làm ngắn video của bạn rồi thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="232"/>
        <source>Unknown error</source>
        <translation>Lỗi không xác định</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="233"/>
        <source>This should not happen...</source>
        <translation>Điều này không nên xảy ra...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="265"/>
        <source>Video processed, adding frames...</source>
        <translation>Đã xử lý video, đang thêm vào các khung hình...</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="301"/>
        <source>Failed import</source>
        <translation>Nhập liệu thất bại</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="302"/>
        <source>Was unable to find internal files, import unsuccessful.</source>
        <translation>Không tìm thấy các tập tin nội tại, nhập liệu không thành công.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="317"/>
        <source>Sound only</source>
        <translation>Chỉ âm thanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="318"/>
        <source>You need to be on a sound layer to import the audio</source>
        <translation>Cần ở trên layer âm thanh để nhập âm thanh</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="330"/>
        <source>Move to an empty frame</source>
        <translation>Chuyển tới một khung hình trống</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="331"/>
        <source>A frame already exists on frame: %1 Move the scrubber to a empty position on the timeline and try again</source>
        <translation>Đã tồn tại một khung hình tại vị trí: %1 Hãy di chuyển scrubber tới vị trí trống trên dòng thời gian và thử lại</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="373"/>
        <source>FFmpeg Not Found</source>
        <translation>Không tìm thấy FFmpeg</translation>
    </message>
    <message>
        <location filename="../core_lib/src/movieimporter.cpp" line="374"/>
        <source>Please place the ffmpeg binary in plugins directory and try again</source>
        <translation>Vui lòng đặt tệp nhị phân ffmpeg vào thư mục plugins rồi thử lại</translation>
    </message>
</context>
<context>
    <name>Object</name>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="524"/>
        <source>error</source>
        <translation>lỗi</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="778"/>
        <source>Black</source>
        <translation>Đen</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="779"/>
        <source>Red</source>
        <translation>Đỏ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="780"/>
        <source>Dark Red</source>
        <translation>Đỏ tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="781"/>
        <source>Orange</source>
        <translation>Cam</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="782"/>
        <source>Dark Orange</source>
        <translation>Cam tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="783"/>
        <source>Yellow</source>
        <translation>Vàng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="784"/>
        <source>Dark Yellow</source>
        <translation>Vàng tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="785"/>
        <source>Green</source>
        <translation>Xanh lá cây</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="786"/>
        <source>Dark Green</source>
        <translation>Xanh lá cây tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="787"/>
        <source>Cyan</source>
        <translation>Lục lam</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="788"/>
        <source>Dark Cyan</source>
        <translation>Lục lam sậm</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="789"/>
        <source>Blue</source>
        <translation>Xanh dương</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="790"/>
        <source>Dark Blue</source>
        <translation>Xanh dương tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="791"/>
        <source>White</source>
        <translation>Trắng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="792"/>
        <source>Very Light Grey</source>
        <translation>Xám sáng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="793"/>
        <source>Light Grey</source>
        <translation>Xám nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="794"/>
        <source>Grey</source>
        <translation>Xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="795"/>
        <source>Dark Grey</source>
        <translation>Xám tối</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="796"/>
        <source>Pale Orange Yellow</source>
        <translation>Vàng cam nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="797"/>
        <source>Pale Grayish Orange Yellow</source>
        <translation>Vàng cam xám nhạt</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="798"/>
        <source>Orange Yellow </source>
        <translation>Vàng cam</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="799"/>
        <source>Grayish Orange Yellow</source>
        <translation>Vàng cam xám</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="800"/>
        <source>Light Orange Yellow</source>
        <translation>Vàng cam nhẹ</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/object.cpp" line="801"/>
        <source>Light Grayish Orange Yellow</source>
        <translation>Vàng cam xám nhẹ</translation>
    </message>
</context>
<context>
    <name>OnionAlignOptionsWidget</name>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="54"/>
        <source>中心对齐</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="54"/>
        <source>前后帧内容中心对齐到中点（等同双击画布）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="55"/>
        <source>复位前帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="55"/>
        <source>归零红色（前帧）幽灵的全部变换（位移/旋转/缩放）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="56"/>
        <source>复位后帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="56"/>
        <source>归零蓝色（后帧）幽灵的全部变换（位移/旋转/缩放）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="57"/>
        <source>全部复位</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/onionalignoptionswidget.cpp" line="57"/>
        <source>清空当前图层全部幽灵变换（等同 Alt+点空白）</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>OnionSkin</name>
    <message>
        <location filename="../app/ui/onionskin.ui" line="23"/>
        <source>Onion Skins</source>
        <comment>Window title of display options like .</comment>
        <translation>Onion Skin</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="111"/>
        <source>Previous Frames</source>
        <translation>Các khung hình trước</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="166"/>
        <location filename="../app/ui/onionskin.ui" line="259"/>
        <source>...</source>
        <translation>...</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="163"/>
        <source>Onion skin color: red</source>
        <translation>Màu onion skin: đỏ</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="192"/>
        <source>Next Frames</source>
        <translation>Các khung hình tiếp theo</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="253"/>
        <source>Onion skin color: blue</source>
        <translation>Màu onion skin: xanh biển</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="285"/>
        <source>Distributed Opacity</source>
        <translation>Độ mờ Phân tán</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="395"/>
        <source>Min</source>
        <translation>Min</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="355"/>
        <location filename="../app/ui/onionskin.ui" line="420"/>
        <source> %</source>
        <translation>%</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="330"/>
        <source>Max</source>
        <translation>Max</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="444"/>
        <source>Show On All Layers</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="458"/>
        <source>Show Keyframes Only</source>
        <translation>Chỉ hiện thị các khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/ui/onionskin.ui" line="465"/>
        <source>Show During Playback</source>
        <translation>Hiển thị khi chạy hoạt ảnh</translation>
    </message>
</context>
<context>
    <name>OverlayPainter</name>
    <message>
        <location filename="../core_lib/src/overlaypainter.cpp" line="215"/>
        <source>Safe Action area %1 %</source>
        <translation>Vùng An toàn Thao tác %1 %</translation>
    </message>
    <message>
        <location filename="../core_lib/src/overlaypainter.cpp" line="242"/>
        <source>Safe Title area %1 %</source>
        <translation>Vùng An toàn cho Tiêu đề %1 %</translation>
    </message>
</context>
<context>
    <name>PaletteExtractDialog</name>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="245"/>
        <location filename="../app/src/tvptoolsdialog.cpp" line="281"/>
        <source>调色板提取</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="251"/>
        <source>颜色数量：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="257"/>
        <source>选择图片并提取</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="261"/>
        <source>提取后将生成色块图层，并将色值复制到剪贴板。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="272"/>
        <source>选择图片</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="281"/>
        <source>无法读取图片：%1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="370"/>
        <source>_调色板</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="379"/>
        <source>生成调色板</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>PegBarAligner</name>
    <message>
        <location filename="../core_lib/src/structure/pegbaraligner.cpp" line="46"/>
        <source>Peg hole not found!
Check selection, and please try again.</source>
        <comment>PegBar error message</comment>
        <translation>Không tìm thấy lỗ cọc!
Kiểm tra vùng chọn và vui lòng thử lại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/structure/pegbaraligner.cpp" line="65"/>
        <source>Peg bar not found at %2, %1</source>
        <translation>Không tìm thấy thanh cọc tại %2, %1</translation>
    </message>
</context>
<context>
    <name>PegBarAlignmentDialog</name>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="14"/>
        <source>Peg bar Alignment</source>
        <translation>Căn chỉnh Thanh cọc</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="36"/>
        <source>Prerequisites</source>
        <translation>Điều kiện tiên quyết</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="43"/>
        <source>1) A selection should exist</source>
        <translation>1) Cần tồn tại một vùng chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="59"/>
        <source>2) The selection be large enough to contain the center pegs of all frames</source>
        <translation>2) Vùng chọn đủ lớn để chứa được lỗ cọc tâm của tất cả các frame</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="75"/>
        <source>3) At least one layer should be selected (Bitmaps only!)</source>
        <translation>3) Cần chọn ít nhất một layer (chỉ layer Bitmap!)</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="87"/>
        <source>Layer selection</source>
        <translation>Chọn layer</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="123"/>
        <source>Reference key:</source>
        <translation>Khóa quan hệ:</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="130"/>
        <source>TextLabel</source>
        <translation>Tên Nhãn</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="170"/>
        <source>Close</source>
        <translation>Đóng</translation>
    </message>
    <message>
        <location filename="../app/ui/pegbaralignmentdialog.ui" line="180"/>
        <source>Align</source>
        <translation>Căn chỉnh</translation>
    </message>
    <message>
        <location filename="../app/src/pegbaralignmentdialog.cpp" line="163"/>
        <source>No layers selected!</source>
        <comment>PegBar Dialog error message</comment>
        <translation>Không có layer nào được chọn!</translation>
    </message>
</context>
<context>
    <name>Pencil2D</name>
    <message>
        <location filename="../app/src/pencil2d.cpp" line="119"/>
        <source>Warning</source>
        <translation>Cảnh báo</translation>
    </message>
    <message>
        <location filename="../app/src/pencil2d.cpp" line="119"/>
        <source>An instance of Pencil2D is already open. Running multiple instances of Pencil2D simultaneously is not recommended and could potentially result in data loss and other unexpected behavior.</source>
        <translation>Một chương trình của Pencil2D đã được mở. Chạy nhiều chương trình của Pencil2D cùng một lúc không được khuyến khích và có thể dẫn đến việc mất dữ liệu hoặc nhiều hành vi không mong muốn.</translation>
    </message>
</context>
<context>
    <name>PredefinedKeySet</name>
    <message>
        <location filename="../app/src/predefinedsetmodel.h" line="65"/>
        <source>Files</source>
        <translation>Tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/predefinedsetmodel.h" line="67"/>
        <source>KeyFrame Pos</source>
        <translation>Vị trí Khung hình chính</translation>
    </message>
</context>
<context>
    <name>PreferencesDialog</name>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="14"/>
        <source>Preferences</source>
        <translation>Cài Đặt chung</translation>
    </message>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="74"/>
        <source>General</source>
        <translation>Tổng quát</translation>
    </message>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="89"/>
        <source>Files</source>
        <translation>Tập tin</translation>
    </message>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="104"/>
        <source>Timeline</source>
        <translation>Dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="119"/>
        <source>Tools</source>
        <translation>Công cụ</translation>
    </message>
    <message>
        <location filename="../app/ui/preferencesdialog.ui" line="134"/>
        <source>Shortcuts</source>
        <translation>Lối tắt</translation>
    </message>
</context>
<context>
    <name>PresetDialog</name>
    <message>
        <location filename="../app/ui/presetdialog.ui" line="14"/>
        <source>Choose a Preset for your Project</source>
        <translation>Chọn một Preset cho dự án của bạn</translation>
    </message>
    <message>
        <location filename="../app/ui/presetdialog.ui" line="20"/>
        <source>&lt;h1&gt;Welcome to Pencil2D!&lt;/h1&gt;</source>
        <translation>&lt;h1&gt;Chào mừng đến với Pencil2D!&lt;/h1&gt; </translation>
    </message>
    <message>
        <location filename="../app/ui/presetdialog.ui" line="27"/>
        <source>Choose a preset to get started:</source>
        <translation>Chọn một preset để bắt đầu:</translation>
    </message>
    <message>
        <location filename="../app/ui/presetdialog.ui" line="37"/>
        <source>Always use this preset</source>
        <translation>Luôn sử dụng preset này</translation>
    </message>
</context>
<context>
    <name>QObject</name>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="42"/>
        <source>大小</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="43"/>
        <source>不透明度</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="44"/>
        <source>无</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="45"/>
        <source>、</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="46"/>
        <source>%1｜直径 %2px｜硬度 %3%
笔尖：%4｜压感控制：%5
%6</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="47"/>
        <source>橡皮</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="47"/>
        <source>画笔</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="50"/>
        <source>圆形</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="50"/>
        <source>方形</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="52"/>
        <source>内置笔刷</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/brushpresetpanel.cpp" line="52"/>
        <source>用户笔刷</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>RecentFileMenu</name>
    <message>
        <location filename="../core_lib/src/interface/recentfilemenu.cpp" line="31"/>
        <source>Clear</source>
        <comment>Clear Recent File menu</comment>
        <translation>Xóa</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/recentfilemenu.cpp" line="32"/>
        <source>Empty</source>
        <comment>Showing when Recent File Menu is empty</comment>
        <translation>Rỗng</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/recentfilemenu.h" line="34"/>
        <source>Open Recent</source>
        <translation>Mở gần đây</translation>
    </message>
</context>
<context>
    <name>RepositionFramesDialog</name>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="14"/>
        <source>Reposition Frames</source>
        <translation>Định vị lại khung hình</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="30"/>
        <source>(Please move selection to desired destination.)</source>
        <translation>(Xin hãy di chuyển vùng lựa chọn đến nơi mong muốn.)</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="43"/>
        <source>Reposition (x,y): </source>
        <translation>Định vị lại trục (x, y):</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="50"/>
        <source>Reposition on other layers?</source>
        <translation>Định vị lại trên những layers khác?</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="57"/>
        <source>Same keyframes as selected</source>
        <translation>Cùng các khung hình chính đã được lựa chọn</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="64"/>
        <source>All keyframes on layer</source>
        <translation>Mọi khung hình chính trên layer</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="93"/>
        <source>Cancel</source>
        <translation>Hủy</translation>
    </message>
    <message>
        <location filename="../app/ui/repositionframesdialog.ui" line="100"/>
        <source>Reposition</source>
        <translation>Định vị lại vị trí</translation>
    </message>
    <message>
        <location filename="../app/src/repositionframesdialog.cpp" line="72"/>
        <source>Repositioned: ( %1, %2 )</source>
        <translation>Định vị lại vị trí: ( %1, %2 )</translation>
    </message>
    <message>
        <location filename="../app/src/repositionframesdialog.cpp" line="78"/>
        <source>Selected on Layer: %1</source>
        <translation>Layer được chọn: %1</translation>
    </message>
    <message>
        <location filename="../app/src/repositionframesdialog.cpp" line="91"/>
        <source>Please move selection to desired destination
or cancel</source>
        <translation>Xin hãy di chuyển vùng lựa chọn đến nơi mong muốn hoặc hủy bỏ</translation>
    </message>
</context>
<context>
    <name>ScribbleArea</name>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="869"/>
        <source>Warning</source>
        <translation>Cảnh báo</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="870"/>
        <source>You are trying to modify a hidden layer! Please select another layer (or make the current layer visible).</source>
        <translation>Bạn đang cố gắng thay đổi một layer ẩn! Vui lòng chọn một layer khác (hoặc hiển thị layer hiện tại).</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="877"/>
        <source>警告</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="878"/>
        <source>该图层已锁定，无法编辑。请点击图层行上的锁图标解锁。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="1572"/>
        <source>Delete Selection</source>
        <comment>Undo Step: clear the selection area.</comment>
        <translation>Xóa vùng chọn</translation>
    </message>
    <message>
        <location filename="../core_lib/src/interface/scribblearea.cpp" line="1588"/>
        <source>Clear Image</source>
        <comment>Undo step text</comment>
        <translation>Làm sạch hình ảnh</translation>
    </message>
</context>
<context>
    <name>ShortcutsPage</name>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="14"/>
        <source>Form</source>
        <translation>Bảng Mẫu</translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="47"/>
        <source>Action:</source>
        <translation>Thao tác: </translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="54"/>
        <source>None</source>
        <translation>Không</translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="61"/>
        <source>Shortcuts:</source>
        <translation>Lối tắt: </translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="73"/>
        <source>Clear</source>
        <translation>Xóa</translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="87"/>
        <source>Save</source>
        <translation>Lưu</translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="94"/>
        <source>Load</source>
        <translation>Tải</translation>
    </message>
    <message>
        <location filename="../app/ui/shortcutspage.ui" line="114"/>
        <source>Restore Default Shortcuts</source>
        <translation>Đặt lại các lối tắt</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="43"/>
        <source>Action</source>
        <comment>Shortcut table header</comment>
        <translation>Thao tác</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="43"/>
        <source>Shortcut</source>
        <comment>Shortcut table header</comment>
        <translation>Lối tắt</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="104"/>
        <source>Shortcut Conflict!</source>
        <translation>Xung đột các lỗi tắt!</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="105"/>
        <source>%1 is already used, overwrite?</source>
        <translation>%1 Đã Được sử dụng, Viết đè lên?</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="139"/>
        <source>Save Pencil2D Shortcut file</source>
        <translation>Lưu tập tin shortcut Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="140"/>
        <source>untitled.pcls</source>
        <translation>untitled.pcls</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="141"/>
        <location filename="../app/src/shortcutspage.cpp" line="167"/>
        <source>Pencil2D Shortcut File(*.pcls)</source>
        <translation>Tập tin Shortcut Pencil2D(*.pcls)</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="165"/>
        <source>Open Pencil2D Shortcut file</source>
        <translation>Mở tập tin Shortcut Pencil2D</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="301"/>
        <source>Add Frame</source>
        <comment>Shortcut</comment>
        <translation>Thêm khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="302"/>
        <source>Clear Frame</source>
        <comment>Shortcut</comment>
        <translation>Xóa khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="303"/>
        <source>Copy</source>
        <comment>Shortcut</comment>
        <translation>Sao chép</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="304"/>
        <source>Paste from Previous Keyframe</source>
        <comment>Shortcut</comment>
        <translation>Dán từ khung hình chính trước</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="305"/>
        <source>Cut</source>
        <comment>Shortcut</comment>
        <translation>Cắt</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="306"/>
        <source>Delete Current Layer</source>
        <comment>Shortcut</comment>
        <translation>Xóa layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="307"/>
        <source>Deselect All</source>
        <comment>Shortcut</comment>
        <translation>Bỏ chọn tất cả</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="308"/>
        <source>Duplicate Frame</source>
        <comment>Shortcut</comment>
        <translation>Sao bản Frame</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="309"/>
        <source>Exit</source>
        <comment>Shortcut</comment>
        <translation>Thoát</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="310"/>
        <source>Export Image</source>
        <comment>Shortcut</comment>
        <translation>Xuất Hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="311"/>
        <source>Export Image Sequence</source>
        <comment>Shortcut</comment>
        <translation>Xuất Chuỗi Hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="312"/>
        <source>Export Movie</source>
        <comment>Shortcut</comment>
        <translation>Xuất Phim</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="314"/>
        <source>Export Palette</source>
        <comment>Shortcut</comment>
        <translation>Xuất Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="317"/>
        <source>Flip In-Between</source>
        <comment>Shortcut</comment>
        <translation>Lật trang In-between</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="318"/>
        <source>Flip Rolling</source>
        <comment>Shortcut</comment>
        <translation>Lật trang Xoay vòng</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="315"/>
        <source>View: Horizontal Flip</source>
        <comment>Shortcut</comment>
        <translation>Xem: Lật theo chiều ngang</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="313"/>
        <source>Export Animated GIF</source>
        <comment>Shortcut</comment>
        <translation>Xuất ảnh động GIF</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="316"/>
        <source>View: Vertical Flip</source>
        <comment>Shortcut</comment>
        <translation>Xem: lật theo chiều dọc</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="319"/>
        <source>Next Frame</source>
        <comment>Shortcut</comment>
        <translation>Khung hình tiếp theo</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="320"/>
        <source>Next Keyframe</source>
        <comment>Shortcut</comment>
        <translation>Khung thính chính tiếp theo</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="321"/>
        <source>Previous Frame</source>
        <comment>Shortcut</comment>
        <translation>Khung hình trước đó</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="322"/>
        <source>Selection: Horizontal Flip</source>
        <comment>Shortcut</comment>
        <translation>Vùng chọn: Lật theo chiều ngang</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="323"/>
        <source>Selection: Vertical Flip</source>
        <comment>Shortcut</comment>
        <translation>Vùng chọn: Lật theo chiều dọc</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="324"/>
        <source>Previous Keyframe</source>
        <comment>Shortcut</comment>
        <translation>Khung hình chính trước đó</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="325"/>
        <source>Selection: Reposition Frames</source>
        <comment>Shortcut</comment>
        <translation>Vùng chọn: Định vị lại các khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="326"/>
        <source>Selection: Add Frame Exposure</source>
        <comment>Shortcut</comment>
        <translation>Vùng Chọn: Tăng Phơi sáng Khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="327"/>
        <source>Selection: Subtract Frame Exposure</source>
        <comment>Shortcut</comment>
        <translation>Vùng Chọn: Giảm Phơi sáng Khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="328"/>
        <source>Selection: Reverse Keyframes</source>
        <comment>Shortcut</comment>
        <translation>Vùng Chọn: Đảo ngược các khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="329"/>
        <source>Selection: Remove Keyframes</source>
        <comment>Shortcut</comment>
        <translation>Vùng chọn: Xóa các khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="330"/>
        <source>Toggle Grid</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Lưới</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="331"/>
        <source>Toggle Center Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật đường xuyên tâm</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="332"/>
        <source>Toggle Thirds Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt đường chia ba</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="333"/>
        <source>Toggle Golden Ratio Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt đường tỉ lệ vàng</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="334"/>
        <source>Toggle Safe Areas Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt vùng an toàn</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="335"/>
        <source>Toggle One Point Perspective Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt phối cảnh 1 điểm</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="336"/>
        <source>Toggle Two Point Perspective Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt phối cảnh 2 điểm</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="337"/>
        <source>Toggle Three Point Perspective Overlay</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt phối cảnh 3 điểm</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="338"/>
        <source>Import Image</source>
        <comment>Shortcut</comment>
        <translation>Nhập Hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="339"/>
        <source>Import Image Sequence</source>
        <comment>Shortcut</comment>
        <translation>Nhập Chuỗi Hình ảnh</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="340"/>
        <source>Import Image Predefined Set</source>
        <comment>Shortcut</comment>
        <translation>Nhập Bộ ảnh Định Trước</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="341"/>
        <source>Import Movie Video</source>
        <comment>Shortcut</comment>
        <translation>Nhập Video</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="342"/>
        <source>Import Movie Audio</source>
        <comment>Shortcut</comment>
        <translation>Nhập Âm thanh của Video</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="343"/>
        <source>Import Animated Image</source>
        <comment>Shortcut</comment>
        <translation>Nhập Ảnh Động</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="344"/>
        <source>Import Layers from project file</source>
        <comment>Shortcut</comment>
        <translation>Nhập các Layer từ Tập tin Dự án</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="345"/>
        <source>Import Palette (Append)</source>
        <comment>Shortcut</comment>
        <translation>Nhập Bảng Màu (Thêm vào)</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="346"/>
        <source>Import Palette (Replace)</source>
        <comment>Shortcut</comment>
        <translation>Nhập Bảng Màu (Thay thế)</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="347"/>
        <source>Import Sound</source>
        <comment>Shortcut</comment>
        <translation>Nhập Âm thanh</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="348"/>
        <source>Show All Layers</source>
        <comment>Shortcut</comment>
        <translation>Hiển thị Mọi Layer</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="349"/>
        <source>Show Current Layer Only</source>
        <comment>Shortcut</comment>
        <translation>Chỉ Hiển thị Layer Hiện hành</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="350"/>
        <source>Show Layers Relative to Current Layer</source>
        <comment>Shortcut</comment>
        <translation>Hiển thị các Layer có quan hệ với Layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="351"/>
        <source>Toggle Loop</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Vòng lặp</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="352"/>
        <source>Toggle Range Playback</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt chạy hoạt hình trong vùng</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="353"/>
        <source>Move Frame Backward</source>
        <comment>Shortcut</comment>
        <translation>Di chuyển Khung hình về phía sau</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="354"/>
        <source>Move Frame Forward</source>
        <comment>Shortcut</comment>
        <translation>Di chuyển Khung hình về phía trước</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="355"/>
        <source>New Bitmap Layer</source>
        <comment>Shortcut</comment>
        <translation>Tạo Layer Bitmap</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="356"/>
        <source>New Camera Layer</source>
        <comment>Shortcut</comment>
        <translation>Tạo Layer Máy quay</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="357"/>
        <source>New File</source>
        <comment>Shortcut</comment>
        <translation>Tạo tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="358"/>
        <source>New Sound Layer</source>
        <comment>Shortcut</comment>
        <translation>Tạo Layer Âm thanh</translation>
    </message>
    <message>
        <source>New Vector Layer</source>
        <comment>Shortcut</comment>
        <translation type="vanished">Tạo Layer Vector</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="359"/>
        <source>Toggle Next Onion Skin</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Onion Skin tiếp theo</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="360"/>
        <source>Toggle Previous Onion Skin</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Onion Skin trước đó</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="361"/>
        <source>Open File</source>
        <comment>Shortcut</comment>
        <translation>Mở Tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="362"/>
        <source>Paste</source>
        <comment>Shortcut</comment>
        <translation>Dán</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="363"/>
        <source>Play/Stop</source>
        <comment>Shortcut</comment>
        <translation>Chạy/Dừng</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="364"/>
        <source>Peg bar Alignment</source>
        <comment>Shortcut</comment>
        <translation>Căn chỉnh Thanh cọc</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="365"/>
        <source>Preferences</source>
        <comment>Shortcut</comment>
        <translation>Cài đặt</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="366"/>
        <source>Redo</source>
        <comment>Shortcut</comment>
        <translation>Hoàn tác</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="367"/>
        <source>Remove Frame</source>
        <comment>Shortcut</comment>
        <translation>Xóa Khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="368"/>
        <source>Reset Windows</source>
        <comment>Shortcut</comment>
        <translation>Đặt lại Cửa sổ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="369"/>
        <source>Lock Windows</source>
        <comment>Shortcut</comment>
        <translation>Khóa Cửa sổ Hiển thị</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="370"/>
        <source>Reset View</source>
        <comment>Shortcut</comment>
        <translation>Đặt lại Chế độ Xem</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="371"/>
        <source>Center View</source>
        <comment>Shortcut</comment>
        <translation>Tâm Chế độ xem</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="372"/>
        <source>Rotate Anticlockwise</source>
        <comment>Shortcut</comment>
        <translation>Xoay Ngược chiều Đồng hồ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="373"/>
        <source>Rotate Clockwise</source>
        <comment>Shortcut</comment>
        <translation>Xoay Theo chiều Đồng hồ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="374"/>
        <source>Reset Rotation</source>
        <comment>Shortcut</comment>
        <translation>Đưa về Góc xoay Mặc định</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="375"/>
        <source>Save File As</source>
        <comment>Shortcut</comment>
        <translation>Lưu Tập tin dưới dạng</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="376"/>
        <source>Save File</source>
        <comment>Shortcut</comment>
        <translation>Lưu Tập tin</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="377"/>
        <source>Select All</source>
        <comment>Shortcut</comment>
        <translation>Chọn tất cả</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="378"/>
        <source>Toggle Status Bar Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Thanh Trạng thái</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="379"/>
        <source>Toggle Color Inspector Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Kiểm tra Màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="380"/>
        <source>Toggle Color Palette Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Bảng màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="381"/>
        <source>Toggle Color Box Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Hộp thoại Màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="382"/>
        <source>Toggle Onion Skins Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Onion Skin</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="383"/>
        <source>Toggle Timeline Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="384"/>
        <source>Toggle Tools Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Công cụ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="385"/>
        <source>Toggle Options Window Visibility</source>
        <comment>Shortcut</comment>
        <translation>Bật tắt Cửa sổ Tùy chọn</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="386"/>
        <source>Brush Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Cọ vẽ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="387"/>
        <source>Bucket Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Xô màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="388"/>
        <source>Eraser Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Tẩy</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="389"/>
        <source>Eyedropper Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Chọn màu</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="390"/>
        <source>Hand Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Bàn tay</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="391"/>
        <source>Move Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Duy chuyển</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="392"/>
        <source>洋葱皮对位工具</source>
        <comment>Shortcut</comment>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="393"/>
        <source>Pen Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Bút mực</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="394"/>
        <source>Pencil Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Bút chì</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="395"/>
        <source>Polyline Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Polyline</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="396"/>
        <source>Select Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ Chọn </translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="397"/>
        <source>Lasso Tool</source>
        <comment>Shortcut</comment>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="398"/>
        <source>Deform Tool</source>
        <comment>Shortcut</comment>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="399"/>
        <source>Smudge Tool</source>
        <comment>Shortcut</comment>
        <translation>Công cụ</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="400"/>
        <source>Reset all tools to default</source>
        <comment>Shortcut</comment>
        <translation>Đặt lại Tất cả Công cụ về Mặc Định</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="401"/>
        <source>Change Line Color (Current keyframe)</source>
        <comment>Shortcut</comment>
        <translation>Thay đổi Màu Nét (Khung hình chính hiện tại)</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="402"/>
        <source>Change Line Color (All keyframes on layer)</source>
        <comment>Shortcut</comment>
        <translation>Thay đổi Màu Nét (Mọi khung hình chính của layer)</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="403"/>
        <source>Change Layer / Keyframe Opacity</source>
        <comment>Shortcut</comment>
        <translation>Thay đổi Độ mờ Layer / Khung hình chính</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="404"/>
        <source>Undo</source>
        <comment>Shortcut</comment>
        <translation>Hoàn tác</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="405"/>
        <source>Set Zoom to 100%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 100%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="406"/>
        <source>Set Zoom to 200%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 200%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="407"/>
        <source>Set Zoom to 25%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 25%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="408"/>
        <source>Set Zoom to 300%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 300%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="409"/>
        <source>Set Zoom to 33%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 33%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="410"/>
        <source>Set Zoom to 400%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 400%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="411"/>
        <source>Set Zoom to 50%</source>
        <comment>Shortcut</comment>
        <translation>Đặt Thu phóng thành 50%</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="412"/>
        <source>Zoom In</source>
        <comment>Shortcut</comment>
        <translation>Zoom</translation>
    </message>
    <message>
        <location filename="../app/src/shortcutspage.cpp" line="413"/>
        <source>Zoom Out</source>
        <comment>Shortcut</comment>
        <translation>Thu nhỏ</translation>
    </message>
</context>
<context>
    <name>Status</name>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="108"/>
        <source>Everything ok.</source>
        <translation>Ôkêla.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="109"/>
        <source>Ooops, Something went wrong.</source>
        <translation>Íiii da, đã xảy ra lỗi.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="110"/>
        <source>File doesn&apos;t exist.</source>
        <translation>Tập tin không tồn tại.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="111"/>
        <source>Cannot open file.</source>
        <translation>Không thể mở tập tin.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="112"/>
        <source>The file is not a valid xml document.</source>
        <translation>Tập tin không phải là tài liệu xml hợp lệ.</translation>
    </message>
    <message>
        <location filename="../core_lib/src/util/pencilerror.cpp" line="113"/>
        <source>The file is not valid pencil document.</source>
        <translation>Tập tin không phải tài liệu pencil hợp lệ.</translation>
    </message>
</context>
<context>
    <name>StatusBar</name>
    <message>
        <location filename="../app/src/statusbar.cpp" line="50"/>
        <source>查看并复制最近的调试日志</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="115"/>
        <location filename="../app/src/statusbar.cpp" line="136"/>
        <source>Click to draw. Hold Ctrl and Shift to erase or Alt to select a color from the canvas.</source>
        <translation>Nhấp để vẽ. Nhấn giữ Ctrl và Shift để xóa hoặc Alt để chọn lấy một màu từ vùng vẽ.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="118"/>
        <source>Click to erase.</source>
        <translation>Nhấp để xóa.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="121"/>
        <source>Click and drag to create or modify a selection. Hold Alt to modify its contents or press Backspace to clear them.</source>
        <translation>Nhấp và kéo chuột để tạo hoặc điều chỉnh một vùng chọn. Nhấn giữ Alt để chỉnh sửa nội dung trong vùng chọn hoặc nhấn Dấu cách để xóa chúng.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="124"/>
        <source>Click and drag to move an object. Hold Ctrl to rotate.</source>
        <translation>Nhấp và kéo chuột để di chuyển đối tượng. Nhấn giữ Ctrl để xoay.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="127"/>
        <source>Click and drag to move the camera. While on in-between frames, drag handle to change interpolation.</source>
        <translation>Chọn và kéo để di chuyển máy quay camera. Trong khi đang ở giữa các khung hình, kéo tay cầm để thay đổi nội suy.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="130"/>
        <source>Click and drag to pan. Hold Ctrl to zoom or Alt to rotate.</source>
        <translation>Nhấp và kéo chuột để di chuyển màn ảnh. Nhấn giữ Ctrl để thu phóng hoặc Alt để xoay.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="133"/>
        <source>Click to liquefy pixels or modify a vector line. Hold Alt to smooth.</source>
        <translation>Nhấp để liquify các pixel hoác hiệu chỉnh các đường vector. Nhấn giữ Alt để làm mượt.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="141"/>
        <source>Click to continue the polyline. Double-click or press enter to complete the line or press Escape to discard it.</source>
        <translation>Nhấp để tiếp diễn polyline. Nhấp đúp chuột hoặc nhấn Enter để kết đường hoặc nhấn Esc để loại bỏ.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="145"/>
        <source>Click to create a new polyline. Hold Ctrl and Shift to erase.</source>
        <translation>Nhấp để tạo đường polyline mới. Nhấn giữ Ctrl và Shift để xóa.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="149"/>
        <source>Click to fill an area with the current color. Hold Alt to select a color from the canvas.</source>
        <translation>Nhấp để đổ màu một khu vực bằng màu hiện hành. Nhấn giữ Alt để chọn lấy một màu từ vùng vẽ.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="152"/>
        <source>Click to select a color from the canvas.</source>
        <translation>Nhấp để chọn lấy một màu từ vùng vẽ.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="155"/>
        <source>Click to paint. Hold Ctrl and Shift to erase or Alt to select a color from the canvas.</source>
        <translation>Nhấp để vẽ màu. Nhấn giữ Ctrl và Shift để xóa hoặc Alt để chọn lấy một màu từ vùng vẽ.</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="158"/>
        <source>拖动绘制自由形状选区；Shift=加选, Alt=减选, Shift+Alt=相交, Ctrl+Alt=对称差, Ctrl=替换（可拖动途中按）；动作与扩展/收缩在工具选项。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="161"/>
        <source>变形工具：液化=笔刷推挤像素，弯曲=拖网格点，笼罩=画轮廓拖顶点，透视=拖四角；模式与参数在工具选项；回车/双击=应用，Esc=取消，Backspace=复位。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="164"/>
        <source>拖动洋葱皮红/蓝幽灵像对位参考；Ctrl+拖动=绕幽灵中心旋转（Shift 吸附步进），Shift+拖动=缩放；双击=前后帧中心自动对齐；Alt+点击=归零该侧，Alt+点空白=清空全部。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="205"/>
        <source>This file has unsaved changes</source>
        <translation>Tập tin này có các thay đổi chưa được lưu</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="209"/>
        <source>This file has no unsaved changes</source>
        <translation>Tập tin này không có thay đổi nào chưa được lưu</translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="228"/>
        <source>调试日志</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="237"/>
        <source>全部复制</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="243"/>
        <source>保存到文件...</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="246"/>
        <source>保存调试日志</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="247"/>
        <source>文本文件 (*.txt)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/statusbar.cpp" line="256"/>
        <source>关闭</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>StrokeOptionsWidget</name>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="14"/>
        <source>Form</source>
        <translation type="unfinished">Bảng Mẫu</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="37"/>
        <source>Set Stroke Width &lt;br&gt;&lt;b&gt;[SHIFT]+drag&lt;/b&gt;&lt;br&gt;for quick adjustment</source>
        <translation type="unfinished">Chỉnh Độ rộng Nét &lt;br&gt;&lt;b&gt;[SHIFT]+kéo&lt;/b&gt;&lt;br&gt;để điều chỉnh nhanh</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="73"/>
        <source>Set Stroke Feather &lt;br&gt;&lt;b&gt;[CTRL]+drag&lt;/b&gt;&lt;br&gt;for quick adjustment</source>
        <translation type="unfinished">Chỉnh Feather Nét &lt;br&gt;&lt;b&gt;[CTRL]+kéo&lt;/b&gt;&lt;br&gt;để điều chỉnh nhanh</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="115"/>
        <source>Stabilizer</source>
        <translation type="unfinished">Bộ ổn định</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="122"/>
        <source>Use stabilizer to interpolate strokes</source>
        <translation type="unfinished">Sử dụng bộ ổn định để nội suy các nét</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="125"/>
        <source>None</source>
        <comment>Stablizer level</comment>
        <translation type="unfinished">Không</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="129"/>
        <source>None</source>
        <comment>Stabilizer option</comment>
        <translation type="unfinished">Không</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="134"/>
        <source>Simple</source>
        <comment>Stabilizer option</comment>
        <translation type="unfinished">Đơn giản</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="139"/>
        <source>Strong</source>
        <comment>Stabilizer option</comment>
        <translation type="unfinished">Mạnh</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="154"/>
        <source>Enable or disable feathering</source>
        <translation type="unfinished">Mở hoặc tắt chế độ feather</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="157"/>
        <source>Use Feather</source>
        <translation type="unfinished">Sử dụng Feather</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="164"/>
        <source>Close Polyline path (hold Ctrl to temporarily invert)</source>
        <translation type="unfinished">Đóng đường polyline (giữ Ctrl để tạm thời đảo)</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="167"/>
        <source>Closed Path</source>
        <translation type="unfinished">Đường kín</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="174"/>
        <source>Use Bézier curves to create curved lines</source>
        <translation type="unfinished">Sử dụng dạng đường cong Bézier để vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="177"/>
        <source>Bézier</source>
        <comment>Tool options</comment>
        <translation type="unfinished">Bézier</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="184"/>
        <source>Vary strokes based on pressure when drawing on a tablet</source>
        <translation type="unfinished">Các nét thay đổi dựa trên lực nhấn khi vẽ trên máy tính bảng</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="187"/>
        <source>Pressure</source>
        <comment>Tool options</comment>
        <translation type="unfinished">Lực nhấn</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="194"/>
        <source>Use anti-aliasing to create smooth edges</source>
        <translation type="unfinished">Sử dụng khử răng cưa để tạo các cạnh mịn</translation>
    </message>
    <message>
        <location filename="../app/ui/strokeoptionswidget.ui" line="197"/>
        <source>Anti-Aliasing</source>
        <comment>Brush AA</comment>
        <translation type="unfinished">Khử răng cưa</translation>
    </message>
    <message>
        <location filename="../app/src/strokeoptionswidget.cpp" line="40"/>
        <source>Width</source>
        <translation type="unfinished">Chiều rộng</translation>
    </message>
    <message>
        <location filename="../app/src/strokeoptionswidget.cpp" line="43"/>
        <source>Feather</source>
        <translation type="unfinished">Làm mềm vùng rìa ảnh</translation>
    </message>
</context>
<context>
    <name>TimeControls</name>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="50"/>
        <source> fps</source>
        <translation>fps</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="51"/>
        <source>Frames per second</source>
        <translation>Khung hình mỗi giây</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="58"/>
        <source>Display timecode</source>
        <comment>Timeline menu for choose a timecode</comment>
        <translation>Hiển thị Mã thời gian</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="61"/>
        <source>No text</source>
        <translation>Không có chữ</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="62"/>
        <source>Frames</source>
        <translation>Hiển thị số khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="63"/>
        <source>SMPTE Timecode</source>
        <translation>Hiển thị Mã thời gian SMPTE</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="64"/>
        <source>SFF Timecode</source>
        <translation>Hiển thị Mã thời gian SFF</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="83"/>
        <location filename="../app/src/timecontrols.cpp" line="400"/>
        <source>Actual frame number</source>
        <translation>Số khung hình thực</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="86"/>
        <location filename="../app/src/timecontrols.cpp" line="418"/>
        <source>Timecode format MM:SS:FF</source>
        <translation>Định dạng Mã thời gian MM:SS:FF</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="89"/>
        <location filename="../app/src/timecontrols.cpp" line="409"/>
        <source>Timecode format S:FF</source>
        <translation>Định dạng Mã thời gian S:FF</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="97"/>
        <source>Playback speed</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="121"/>
        <source>Measured frames per second during playback</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="129"/>
        <source>Start of playback loop</source>
        <translation>Bắt Đầu của một vòng lập</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="137"/>
        <source>End of playback loop</source>
        <translation>Kết thúc của một vòng lập</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="140"/>
        <source>Range</source>
        <translation>Khoảng vùng</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="142"/>
        <source>Playback range</source>
        <translation>Khoảng phát lại</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="171"/>
        <location filename="../app/src/timecontrols.cpp" line="322"/>
        <source>Play</source>
        <translation>Chạy</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="172"/>
        <source>Loop</source>
        <translation>Vòng lặp</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="173"/>
        <source>声音开/关</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="174"/>
        <source>擦洗时间轴时播放声音</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Sound on/off</source>
        <translation type="vanished">Âm thanh mở/tắt</translation>
    </message>
    <message>
        <source>Sound scrub on/off</source>
        <translation type="vanished">Scrub âm thanh mở/tắt</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="175"/>
        <source>Jump to the End</source>
        <comment>Tooltip of the jump to end button</comment>
        <translation>Nhảy tới cuối</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="176"/>
        <source>Jump to the Start</source>
        <comment>Tooltip of the jump to start button</comment>
        <translation>Nhảy tới đầu</translation>
    </message>
    <message>
        <location filename="../app/src/timecontrols.cpp" line="317"/>
        <source>Stop</source>
        <translation>Ngưng lại</translation>
    </message>
</context>
<context>
    <name>TimeLine</name>
    <message>
        <location filename="../app/src/timeline.cpp" line="55"/>
        <source>Timeline</source>
        <comment>Subpanel title</comment>
        <translation>Dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="81"/>
        <source>Layers:</source>
        <translation>Layers: </translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="86"/>
        <source>Add Layer</source>
        <translation>Thêm Layer</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="92"/>
        <source>Delete Layer</source>
        <translation>Xóa Layer</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="98"/>
        <source>Duplicate Layer</source>
        <translation>Nhân đôi lớp layer</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="106"/>
        <source>在原图层上方复制一个同结构图层，关键帧内容全部为空白（清稿/描线用）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="113"/>
        <source>全部图层可见性切换（全开→全关，有关→全开）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="119"/>
        <source>全部图层锁定切换（全解锁→全锁，有锁→全解锁）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="125"/>
        <source>工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="126"/>
        <source>口型同步 / 调色板提取 / 视频抽帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="129"/>
        <source>口型同步切换器</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="130"/>
        <source>调色板提取</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="131"/>
        <source>视频抽帧中割</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="151"/>
        <source>New Bitmap Layer</source>
        <translation>Layer Bitmap mới</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="196"/>
        <source>Hold 1 frame per key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="201"/>
        <source>Hold 2 frames per key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="206"/>
        <source>Hold 3 frames per key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="211"/>
        <source>Hold 4 frames per key</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="226"/>
        <source>循环克隆次数</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="231"/>
        <source>循环</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="232"/>
        <source>循环克隆帧：把选中的帧（未选中则整层）按原间隔重复指定次数</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="687"/>
        <source>一拍 %1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="715"/>
        <source>%1_清空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="735"/>
        <source>复制图层并清空</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="787"/>
        <source>循环克隆 ×%1</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>New Vector Layer</source>
        <translation type="vanished">Layer Vector mới</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="152"/>
        <source>New Sound Layer</source>
        <translation>Layer âm thanh mới</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="153"/>
        <source>New Camera Layer</source>
        <translation>Layer máy quay mới</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="154"/>
        <source>New Colorize Layer</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="156"/>
        <source>Layer</source>
        <comment>Timeline add-layer menu</comment>
        <translation>Layer</translation>
    </message>
    <message>
        <source>Keys:</source>
        <translation type="vanished">Keys:</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="178"/>
        <source>Add Frame</source>
        <translation>Thêm Khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="184"/>
        <source>Remove Frame</source>
        <translation>Gỡ bỏ Khung hình</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="190"/>
        <source>Duplicate Frame</source>
        <translation>Sao chép Khung hình</translation>
    </message>
    <message>
        <source>Zoom:</source>
        <translation type="vanished">Thu phóng:</translation>
    </message>
    <message>
        <location filename="../app/src/timeline.cpp" line="239"/>
        <source>Adjust frame width</source>
        <translation>Điều chỉnh chiều rộng khung hình</translation>
    </message>
</context>
<context>
    <name>TimeLineCells</name>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="1972"/>
        <source>新建 %1 帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2049"/>
        <source>拉伸帧块</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2079"/>
        <source>移动帧</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2108"/>
        <source>重排图层</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2212"/>
        <source>Layer Properties</source>
        <translation>Thông tin Layer</translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2213"/>
        <source>Layer name:</source>
        <translation>Tên layer:</translation>
    </message>
    <message>
        <location filename="../app/src/timelinecells.cpp" line="2346"/>
        <source>跨层移动帧</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>TimelinePage</name>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="38"/>
        <source>Timeline</source>
        <translation>Dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="58"/>
        <source>Timeline length:</source>
        <comment>Preferences</comment>
        <translation>Độ dài dòng thời gian</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="77"/>
        <source>Short scrub</source>
        <translation>Scrub đoạn ngắn</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="87"/>
        <source>Drawing</source>
        <translation>Vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="93"/>
        <source>When drawing on an empty frame:</source>
        <translation>Khi vẽ trên một khung hình trống:</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="100"/>
        <source>Create a new (blank) key-frame and start drawing on it.</source>
        <translation>Tạo khung hình chính (trống) mới và bắt đầu vẽ.</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="103"/>
        <source>Create a new (blank) key-frame</source>
        <translation>Khởi tạo khung hình chính (trống) mới</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="113"/>
        <source>Duplicate the previous key-frame and start drawing on the duplicate.</source>
        <translation>Sao bản khung hình chính trước và bắt đầu vẽ trên bản sao.</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="116"/>
        <source>Duplicate the previous key-frame</source>
        <translation>Sao bản khung hình chính trước</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="123"/>
        <source>Keep drawing on the previous key-frame</source>
        <translation>Tiếp tục vẽ trên khung hình chính trước</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="130"/>
        <source>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;(Applies to Pencil, Eraser, Pen, Polyline, Bucket and Brush tools)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</source>
        <translation>&lt;html&gt;&lt;head/&gt;&lt;body&gt;&lt;p&gt;(Áp dụng cho các công cụ Bút chì, Tẩy xóa, Bút mực, Polyline, Xô màu và Cọ vẽ)&lt;/p&gt;&lt;/body&gt;&lt;/html&gt;</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="143"/>
        <source>Flip and Roll</source>
        <translation>Lật trang và Xoay vòng</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="151"/>
        <source>Maximum numbers of drawings in roll</source>
        <translation>Số lượng bản vẽ tối đa trong cuộn</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="203"/>
        <source>Msecs per drawing in flip inbetween</source>
        <translation>Mili giây mỗi bức họa của chức năng lật trang inbetween</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="252"/>
        <source>Msecs per drawing in flip roll</source>
        <translation>Mili giây mỗi bức họa của chức năng lật trang xoay vòng</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="264"/>
        <source>Sound scrub</source>
        <translation>Scrub âm thanh</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="297"/>
        <source> ms</source>
        <translation>ms</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="321"/>
        <source>Layer Visibility</source>
        <translation>Độ hiển thị Layer</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="327"/>
        <source>Startup option</source>
        <translation>Tùy chọn Khởi động</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="335"/>
        <source>Current layer only</source>
        <translation>Chỉ layer hiện hành</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="340"/>
        <source>Relative</source>
        <translation>Tương đối</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="345"/>
        <source>All Layers</source>
        <translation>Mọi Layer</translation>
    </message>
    <message>
        <location filename="../app/ui/timelinepage.ui" line="353"/>
        <source>When layer visibility is relative (gray dot)</source>
        <translation>Khi khả năng hiển thị của layer là tương đối (chấm xám)</translation>
    </message>
</context>
<context>
    <name>ToolBoxDockWidget</name>
    <message>
        <location filename="../app/src/toolbox.cpp" line="43"/>
        <source>Tools</source>
        <comment>Window title of Tools</comment>
        <translation>Công cụ</translation>
    </message>
</context>
<context>
    <name>ToolBoxWidget</name>
    <message>
        <location filename="../app/ui/toolboxwidget.ui" line="20"/>
        <source>Tools</source>
        <comment>Window title of tool box</comment>
        <translation>Các công cụ</translation>
    </message>
    <message>
        <location filename="../app/ui/toolboxwidget.ui" line="223"/>
        <source>洋葱皮对位工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/toolboxwidget.ui" line="450"/>
        <source>Smudge</source>
        <translation>Làm mờ</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="131"/>
        <source>Pencil Tool (%1): Sketch with pencil</source>
        <translation>Công cụ bút chì (%1): Vẽ như một bút chì</translation>
    </message>
    <message>
        <source>Select Tool (%1): Select an object</source>
        <translation type="vanished">Công cụ chọn (%1): Chọn một Đối tượng</translation>
    </message>
    <message>
        <source>Move Tool (%1): Move an object</source>
        <translation type="vanished">Công cụ di chuyển (%1): Di chuyển một Đối tượng</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="133"/>
        <source>洋葱皮对位工具 (%1)：拖动移动红/蓝幽灵，Ctrl=旋转，Shift=缩放；双击=中心对齐；Alt+点击=归零</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="135"/>
        <source>Hand Tool (%1): Move the canvas</source>
        <translation>Công cụ bàn tay (%1): Di chuyển vùng vẽ</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="137"/>
        <source>Pen Tool (%1): Sketch with pen</source>
        <translation>Công cụ bút mực (%1): Vẽ với bút mực</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="139"/>
        <source>Eraser Tool (%1): Erase</source>
        <translation>Công cụ tẩy (%1): Tẩy xóa</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="141"/>
        <source>Polyline Tool (%1): Create line/curves</source>
        <translation>Công cụ Polyline (%1): Vẽ Đường thẳng/Đường cong</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="143"/>
        <source>Paint Bucket Tool (%1): Fill selected area with a color</source>
        <translation>Công cụ Xô màu (%1): Tô màu một vùng vẽ với màu sắc chỉ định</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="145"/>
        <source>Brush Tool (%1): Paint smooth stroke with a brush</source>
        <translation>Công cụ cọ vẽ (%1): Vẽ một Đường mượt mà với cọ</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="147"/>
        <source>Eyedropper Tool (%1): Set color from the stage&lt;br&gt;[ALT] for instant access</source>
        <translation>Công cụ chọn màu (%1): Lấy màu từ một mẫu&lt;br&gt;[ALT] truy cập ngay lập tức</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="150"/>
        <source>Smudge Tool (%1):&lt;br&gt;Edit polyline/curves&lt;br&gt;Liquify bitmap pixels&lt;br&gt; (%1)+[Alt]: Smooth</source>
        <translation>Công cụ làm mờ (%1):&lt;br&gt;Chỉnh sửa Polyline/Đường cong&lt;br&gt;Pixels ảnh có Pixel hiệu ứng chất lỏng&lt;br&gt;(%1)+[Alt]: Mềm mại</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="154"/>
        <source>Pencil Tool (%1)</source>
        <translation>Công cụ bút chì (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="156"/>
        <source>Select a free-form (lasso) or rectangular area; press and hold the button to switch variants</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="157"/>
        <source>Deform or move; press and hold the button to switch variants</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="158"/>
        <source>洋葱皮对位工具 (%1)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="453"/>
        <source>%1（%2）：%3</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="481"/>
        <source>%1（%2）：%3；长按此按钮可切换同类工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="508"/>
        <source>矩形选择工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="509"/>
        <source>套索工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="510"/>
        <source>变形工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="511"/>
        <source>移动工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="512"/>
        <source>工具</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="520"/>
        <source>拖拽框选区域</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="521"/>
        <source>圈选任意形状区域</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="522"/>
        <source>自由/液化/弯曲/笼罩/透视（见工具选项）</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="523"/>
        <source>移动对象，相机层上为移动相机</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <source>Select Tool (%1)</source>
        <translation type="vanished">Công cụ chọn (%1)</translation>
    </message>
    <message>
        <source>Move Tool (%1)</source>
        <translation type="vanished">Công cụ di chuyển (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="160"/>
        <source>Hand Tool (%1)</source>
        <translation>Công cụ bàn tay (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="162"/>
        <source>Pen Tool (%1)</source>
        <translation>Công cụ bút mực (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="164"/>
        <source>Eraser Tool (%1)</source>
        <translation>Công cụ gôm tẩy (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="166"/>
        <source>Polyline Tool (%1)</source>
        <translation>Công cụ Polyline (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="168"/>
        <source>Paint Bucket Tool (%1)</source>
        <translation>Công cụ xô màu (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="170"/>
        <source>Brush Tool (%1)</source>
        <translation>Công cụ cọ vẽ (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="172"/>
        <source>Eyedropper Tool (%1)</source>
        <translation>Công cụ chọn màu (%1)</translation>
    </message>
    <message>
        <location filename="../app/src/toolboxwidget.cpp" line="174"/>
        <source>Smudge Tool (%1)</source>
        <translation>Công cụ làm mờ (%1)</translation>
    </message>
</context>
<context>
    <name>ToolOptionWidget</name>
    <message>
        <location filename="../app/src/tooloptionwidget.cpp" line="42"/>
        <source>Options</source>
        <comment>Window title of tool option panel like pen width, feather etc..</comment>
        <translation>Tùy chọn</translation>
    </message>
    <message>
        <source>Width</source>
        <translation type="vanished">Chiều rộng</translation>
    </message>
    <message>
        <source>Feather</source>
        <translation type="vanished">Làm mềm vùng rìa ảnh</translation>
    </message>
</context>
<context>
    <name>ToolOptions</name>
    <message>
        <location filename="../app/ui/tooloptions.ui" line="20"/>
        <source>Form</source>
        <translation>Bảng Mẫu</translation>
    </message>
    <message>
        <source>Set Stroke Width &lt;br&gt;&lt;b&gt;[SHIFT]+drag&lt;/b&gt;&lt;br&gt;for quick adjustment</source>
        <translation type="vanished">Chỉnh Độ rộng Nét &lt;br&gt;&lt;b&gt;[SHIFT]+kéo&lt;/b&gt;&lt;br&gt;để điều chỉnh nhanh</translation>
    </message>
    <message>
        <source>Set Stroke Feather &lt;br&gt;&lt;b&gt;[CTRL]+drag&lt;/b&gt;&lt;br&gt;for quick adjustment</source>
        <translation type="vanished">Chỉnh Feather Nét &lt;br&gt;&lt;b&gt;[CTRL]+kéo&lt;/b&gt;&lt;br&gt;để điều chỉnh nhanh</translation>
    </message>
    <message>
        <source>Enable or disable feathering</source>
        <translation type="vanished">Mở hoặc tắt chế độ feather</translation>
    </message>
    <message>
        <source>Use Feather</source>
        <translation type="vanished">Sử dụng Feather</translation>
    </message>
    <message>
        <source>Show Size and Diff.</source>
        <translation type="vanished">Hiển thị kích thước và sự khác nhau</translation>
    </message>
    <message>
        <source>Contour will be filled</source>
        <translation type="vanished">Đường viền sẽ được lấp màu</translation>
    </message>
    <message>
        <source>Fill Contour</source>
        <translation type="vanished">Lấp màu đường viền</translation>
    </message>
    <message>
        <source>Close Polyline path (hold Ctrl to temporarily invert)</source>
        <translation type="vanished">Đóng đường polyline (giữ Ctrl để tạm thời đảo)</translation>
    </message>
    <message>
        <source>Closed Path</source>
        <translation type="vanished">Đường kín</translation>
    </message>
    <message>
        <source>Use Bézier curves to create curved lines</source>
        <translation type="vanished">Sử dụng dạng đường cong Bézier để vẽ</translation>
    </message>
    <message>
        <source>Bézier</source>
        <comment>Tool options</comment>
        <translation type="vanished">Bézier</translation>
    </message>
    <message>
        <source>Vary strokes based on pressure when drawing on a tablet</source>
        <translation type="vanished">Các nét thay đổi dựa trên lực nhấn khi vẽ trên máy tính bảng</translation>
    </message>
    <message>
        <source>Pressure</source>
        <comment>Tool options</comment>
        <translation type="vanished">Lực nhấn</translation>
    </message>
    <message>
        <source>Use anti-aliasing to create smooth edges</source>
        <translation type="vanished">Sử dụng khử răng cưa để tạo các cạnh mịn</translation>
    </message>
    <message>
        <source>Anti-Aliasing</source>
        <comment>Brush AA</comment>
        <translation type="vanished">Khử răng cưa</translation>
    </message>
    <message>
        <source>Make invisible</source>
        <translation type="vanished">Làm tàng hình</translation>
    </message>
    <message>
        <source>Invisible</source>
        <comment>Tool options</comment>
        <translation type="vanished">Tàng hình</translation>
    </message>
    <message>
        <source>Preserve Alpha</source>
        <translation type="vanished">Bảo tồn kênh Alpha</translation>
    </message>
    <message>
        <source>Alpha</source>
        <comment>Tool options</comment>
        <translation type="vanished">Alpha</translation>
    </message>
    <message>
        <source>Merge vector lines when they are close together</source>
        <translation type="vanished">Hợp nhất các đường vectơ khi chúng gần nhau</translation>
    </message>
    <message>
        <source>Merge</source>
        <comment>Vector line merge (Tool options)</comment>
        <translation type="vanished">Hợp nhất</translation>
    </message>
    <message>
        <source>Stabilizer</source>
        <translation type="vanished">Bộ ổn định</translation>
    </message>
    <message>
        <source>Use stabilizer to interpolate strokes</source>
        <translation type="vanished">Sử dụng bộ ổn định để nội suy các nét</translation>
    </message>
    <message>
        <source>None</source>
        <comment>Stablizer level</comment>
        <translation type="vanished">Không</translation>
    </message>
    <message>
        <source>None</source>
        <comment>Stabilizer option</comment>
        <translation type="vanished">Không</translation>
    </message>
    <message>
        <source>Simple</source>
        <comment>Stabilizer option</comment>
        <translation type="vanished">Đơn giản</translation>
    </message>
    <message>
        <source>Strong</source>
        <comment>Stabilizer option</comment>
        <translation type="vanished">Mạnh</translation>
    </message>
</context>
<context>
    <name>ToolsPage</name>
    <message>
        <location filename="../app/ui/toolspage.ui" line="44"/>
        <source>Brush Tools</source>
        <translation>Công cụ cọ vẽ</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="50"/>
        <source>Use Quick Sizing</source>
        <translation>Sử dụng chức năng Điều chỉnh kích thước nhanh</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="60"/>
        <source>Move Tool</source>
        <translation>Công cụ Di chuyển</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="66"/>
        <source>Rotation snap increment</source>
        <translation>Gia số Xoay chụp</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="89"/>
        <source>15 degrees</source>
        <translation>15 độ</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="99"/>
        <source>Hand Tool</source>
        <translation>Công cụ Bàn tay</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="117"/>
        <source>Zoom in by dragging the cursor up instead of down</source>
        <translation>Phóng to bằng cách cuộn chuột lên thay vì cuộn xuống</translation>
    </message>
    <message>
        <location filename="../app/ui/toolspage.ui" line="120"/>
        <source>Invert Zoom Direction</source>
        <translation>Đảo ngược hướng thu phóng</translation>
    </message>
    <message>
        <location filename="../app/src/toolspage.cpp" line="71"/>
        <source>%1 degrees</source>
        <translation>%1 độ</translation>
    </message>
</context>
<context>
    <name>TransformOptionsWidget</name>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="14"/>
        <source>Form</source>
        <translation type="unfinished">Bảng Mẫu</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="37"/>
        <source>Enable or disable feathering</source>
        <translation type="unfinished">Mở hoặc tắt chế độ feather</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="40"/>
        <source>Show Size and Diff.</source>
        <translation type="unfinished">Hiển thị kích thước và sự khác nhau</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="47"/>
        <source>Anti-Aliasing</source>
        <translation type="unfinished">Khử răng cưa</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="59"/>
        <location filename="../app/ui/transformoptionswidget.ui" line="186"/>
        <source>Mode</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="67"/>
        <source>Free</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="72"/>
        <source>Liquify</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="77"/>
        <source>Warp</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="82"/>
        <source>Cage</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="87"/>
        <source>Perspective</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="103"/>
        <source>Action</source>
        <translation type="unfinished">Thao tác</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="111"/>
        <source>Replace</source>
        <translation type="unfinished">Thay thế</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="116"/>
        <source>Add</source>
        <translation type="unfinished">Thêm</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="121"/>
        <source>Subtract</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="126"/>
        <source>Intersect</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="131"/>
        <source>Symmetric Difference</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="139"/>
        <source>Grow / Shrink</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="169"/>
        <source>Drag corners/edges to scale, drag inside the frame to move, drag the rings outside the corners to rotate. Shift keeps the aspect ratio, Ctrl snaps rotation to 15°. Enter/double-click applies, Esc cancels.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="194"/>
        <source>Move</source>
        <translation type="unfinished">Di chuyển</translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="199"/>
        <source>Scale</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="204"/>
        <source>Rotate</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="209"/>
        <source>Offset</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="214"/>
        <source>Undo (restore)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="222"/>
        <source>Brush Size</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="242"/>
        <source>Amount</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="259"/>
        <source>Reverse Direction</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="273"/>
        <source>Grid Density</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="293"/>
        <source>Flexibility (Alpha)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="310"/>
        <source>Warp Mode</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="318"/>
        <source>Affine</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="323"/>
        <source>Similitude</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="328"/>
        <source>Rigid</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="340"/>
        <source>Drag to draw a cage around the area, then drag its vertices to deform. Enter/double-click applies, Esc cancels.</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/ui/transformoptionswidget.ui" line="354"/>
        <source>Drag the four corners to adjust the perspective. Enter/double-click applies, Esc cancels.</source>
        <translation type="unfinished"></translation>
    </message>
</context>
<context>
    <name>UndoRedoManager</name>
    <message>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="303"/>
        <source>Undo</source>
        <translation>Hoàn tác</translation>
    </message>
    <message>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="323"/>
        <source>Redo</source>
        <translation>Thực hiện lại</translation>
    </message>
    <message>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="344"/>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="350"/>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="353"/>
        <source>Undo</source>
        <comment>Menu item text</comment>
        <translation>Hoàn tác</translation>
    </message>
    <message>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="370"/>
        <location filename="../core_lib/src/managers/undoredomanager.cpp" line="377"/>
        <source>Redo</source>
        <comment>Menu item text</comment>
        <translation>Thực hiện lại</translation>
    </message>
</context>
<context>
    <name>VideoExtractDialog</name>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="404"/>
        <location filename="../app/src/tvptoolsdialog.cpp" line="513"/>
        <location filename="../app/src/tvptoolsdialog.cpp" line="540"/>
        <source>视频抽帧中割</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="411"/>
        <source>ffmpeg：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="419"/>
        <source>视频：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="422"/>
        <source>浏览</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="424"/>
        <source>探测</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="434"/>
        <location filename="../app/src/tvptoolsdialog.cpp" line="643"/>
        <source>播放</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="436"/>
        <source>帧 0 / 0</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="449"/>
        <source>起始帧：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="452"/>
        <source>结束帧：</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="455"/>
        <source>导入到时间轴</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="491"/>
        <source>选择视频</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="493"/>
        <source>视频文件 (*.mp4 *.avi *.mov *.mkv *.webm *.gif)</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="514"/>
        <source>无法运行 ffmpeg/ffprobe，请在上方填写正确的 ffmpeg 路径。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="540"/>
        <source>无法解析视频时长或帧率。</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="566"/>
        <source>帧 %1 / %2</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="648"/>
        <source>暂停</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="684"/>
        <source>AI中割</source>
        <translation type="unfinished"></translation>
    </message>
    <message>
        <location filename="../app/src/tvptoolsdialog.cpp" line="705"/>
        <source>导入视频帧 %1 张</source>
        <translation type="unfinished"></translation>
    </message>
</context>
</TS>

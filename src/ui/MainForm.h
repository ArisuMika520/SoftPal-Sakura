#pragma once

#include "../core/core_exports.h"

#include <vcclr.h>
#include <vector>

#ifdef ExtractAssociatedIcon
#undef ExtractAssociatedIcon
#endif

#using < System.dll>
#using < System.Drawing.dll>
#using < System.Windows.Forms.dll>

namespace softpal
{

    using namespace System;
    using namespace System::ComponentModel;
    using namespace System::Diagnostics;
    using namespace System::Drawing;
    using namespace System::Drawing::Drawing2D;
    using namespace System::IO;
    using namespace System::Reflection;
    using namespace System::Runtime::InteropServices;
    using namespace System::Windows::Forms;

public
    enum class UiWorkKind
    {
        Extract = 0,
        DisassembleScript = 1,
        RebuildScript = 2,
        PalDecrypt = 3,
    };

    ref class WorkRequest
    {
    public:
        UiWorkKind kind = UiWorkKind::Extract;
        array<String ^> ^ files;
        String ^ scriptInPath = String::Empty;
        String ^ textInPath = String::Empty;
        String ^ jsonPath = String::Empty;
        String ^ scriptOutPath = String::Empty;
        String ^ textOutPath = String::Empty;
        String ^ encoding = L"gbk";
        String ^ palInPath = String::Empty;
        String ^ palOutPath = String::Empty;
    };

    ref class WorkResult
    {
    public:
        int handledCount = 0;
        int successCount = 0;
        int failureCount = 0;
        String ^ summary = String::Empty;
        String ^ detail = String::Empty;
        String ^ primaryPath = String::Empty;
        String ^ secondaryPath = String::Empty;
    };

    // 配色取自内嵌背景图：藏青军帽 + 酒红瞳 + 米白底的银发少女
    //   Primary 藏青取自帽子，Accent / Secondary 酒红取自瞳色
    //   卡片均刻意保留高透明度，让背景艺术稿透过来
    ref class Palette abstract sealed
    {
    public:
        static initonly Color WindowBg = Color::FromArgb(0xF1, 0xEE, 0xEA);
        static initonly Color HeaderBg = Color::FromArgb(0xEC, 0xE8, 0xE3);
        static initonly Color CardBg = Color::FromArgb(0xFF, 0xFF, 0xFF);
        static initonly Color CardBorder = Color::FromArgb(0xC2, 0xC6, 0xD1);
        static initonly Color DropBg = Color::FromArgb(0xF6, 0xF4, 0xF1);
        static initonly Color DropBorder = Color::FromArgb(0x9C, 0xA1, 0xB2);
        static initonly Color Primary = Color::FromArgb(0x2D, 0x33, 0x49);
        static initonly Color PrimaryHover = Color::FromArgb(0x3F, 0x47, 0x63);
        static initonly Color PrimaryText = Color::FromArgb(0xFA, 0xF6, 0xF0);
        static initonly Color Secondary = Color::FromArgb(0x9A, 0x2E, 0x45);
        static initonly Color SecondaryHover = Color::FromArgb(0xB3, 0x42, 0x5B);
        static initonly Color Ghost = Color::FromArgb(0xE4, 0xE1, 0xDB);
        static initonly Color GhostHover = Color::FromArgb(0xCF, 0xCB, 0xC4);
        static initonly Color GhostText = Color::FromArgb(0x2D, 0x33, 0x49);
        static initonly Color TextPrimary = Color::FromArgb(0x1F, 0x24, 0x36);
        static initonly Color TextSecondary = Color::FromArgb(0x55, 0x5B, 0x6E);
        static initonly Color Accent = Color::FromArgb(0x8E, 0x29, 0x3F);
        static initonly Color BandOverlay = Color::FromArgb(110, 0xFA, 0xF6, 0xF0);
    };

    // 把父控件的绘制结果回放到自身，让窗体背景图与遮罩条能透过圆角与缝隙显现
    ref class TransparentPanel : public Panel
    {
    public:
        TransparentPanel()
        {
            this->SetStyle(ControlStyles::SupportsTransparentBackColor | ControlStyles::OptimizedDoubleBuffer | ControlStyles::AllPaintingInWmPaint | ControlStyles::UserPaint, true);
            this->BackColor = Color::Transparent;
            this->DoubleBuffered = true;
        }

    protected:
        void OnPaintBackground(PaintEventArgs ^ e) override
        {
            if (this->Parent == nullptr)
                return;
            auto state = e->Graphics->Save();
            e->Graphics->TranslateTransform((float)-this->Left, (float)-this->Top);
            System::Drawing::Rectangle parentRect(this->Left, this->Top, this->Width, this->Height);
            PaintEventArgs ^ pe = gcnew PaintEventArgs(e->Graphics, parentRect);
            this->InvokePaintBackground(this->Parent, pe);
            this->InvokePaint(this->Parent, pe);
            e->Graphics->Restore(state);
        }
    };

    // 圆角卡片：半透明填充 + 软描边，背景图同样透过空白处显现
    ref class RoundedCard : public Panel
    {
    public:
        int CornerRadius = 14;
        Color BorderColor = Palette::CardBorder;
        Color FillColor = Palette::CardBg;
        int BorderThickness = 1;

        RoundedCard()
        {
            this->DoubleBuffered = true;
            this->BackColor = Color::Transparent;
            this->SetStyle(ControlStyles::SupportsTransparentBackColor | ControlStyles::AllPaintingInWmPaint | ControlStyles::OptimizedDoubleBuffer | ControlStyles::ResizeRedraw | ControlStyles::UserPaint, true);
        }

        static GraphicsPath ^ BuildPath(System::Drawing::Rectangle r, int radius)
        {
            int d = radius * 2;
            if (d > r.Width)
                d = r.Width;
            if (d > r.Height)
                d = r.Height;
            GraphicsPath ^ p = gcnew GraphicsPath();
            p->AddArc((float)r.X, (float)r.Y, (float)d, (float)d, 180.0f, 90.0f);
            p->AddArc((float)(r.Right - d), (float)r.Y, (float)d, (float)d, 270.0f, 90.0f);
            p->AddArc((float)(r.Right - d), (float)(r.Bottom - d), (float)d, (float)d, 0.0f, 90.0f);
            p->AddArc((float)r.X, (float)(r.Bottom - d), (float)d, (float)d, 90.0f, 90.0f);
            p->CloseFigure();
            return p;
        }

    protected:
        void OnPaint(PaintEventArgs ^ e) override
        {
            Graphics ^ g = e->Graphics;
            g->SmoothingMode = SmoothingMode::AntiAlias;
            System::Drawing::Rectangle rect(0, 0, this->Width - 1, this->Height - 1);
            GraphicsPath ^ path = BuildPath(rect, CornerRadius);
            SolidBrush ^ fill = gcnew SolidBrush(FillColor);
            g->FillPath(fill, path);
            delete fill;
            Pen ^ pen = gcnew Pen(BorderColor, (float)BorderThickness);
            g->DrawPath(pen, path);
            delete pen;
            delete path;
        }
        void OnPaintBackground(PaintEventArgs ^ e) override
        {
            if (this->Parent != nullptr)
            {
                auto state = e->Graphics->Save();
                e->Graphics->TranslateTransform((float)-this->Left, (float)-this->Top);
                System::Drawing::Rectangle parentRect(this->Left, this->Top, this->Width, this->Height);
                PaintEventArgs ^ pe = gcnew PaintEventArgs(e->Graphics, parentRect);
                this->InvokePaintBackground(this->Parent, pe);
                this->InvokePaint(this->Parent, pe);
                e->Graphics->Restore(state);
            }
            else
            {
                e->Graphics->Clear(Palette::WindowBg);
            }
        }
    };

    // 关键设计：BackColor 永远保持 Color::Transparent，可见填色在 OnPaint
    // 沿圆角路径绘制。若把当前色塞进 BackColor，系统会在我们绘制前先填满
    // 控件矩形，圆角外侧就会露出 4 个深色角块（"黑角"问题）；同理把
    // FlatAppearance 的 MouseOver/Down/Checked 都置透明
    ref class SoftButton : public Button
    {
    public:
        Color BaseColor;
        Color HoverColor;
        Color TextColor;
        int CornerRadius = 10;

        SoftButton()
        {
            BaseColor = Palette::Primary;
            HoverColor = Palette::PrimaryHover;
            TextColor = Palette::PrimaryText;
            // SetStyle 必须先于 BackColor=Transparent；否则 Button 基类会抛 ArgumentException
            this->SetStyle(ControlStyles::SupportsTransparentBackColor | ControlStyles::AllPaintingInWmPaint | ControlStyles::OptimizedDoubleBuffer | ControlStyles::ResizeRedraw | ControlStyles::UserPaint, true);
            this->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            this->FlatAppearance->BorderSize = 0;
            this->FlatAppearance->MouseOverBackColor = Color::Transparent;
            this->FlatAppearance->MouseDownBackColor = Color::Transparent;
            this->FlatAppearance->CheckedBackColor = Color::Transparent;
            this->Cursor = Cursors::Hand;
            this->DoubleBuffered = true;
            this->BackColor = Color::Transparent;
            this->TextAlign = ContentAlignment::MiddleCenter;
            this->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.0f, FontStyle::Bold);
            this->MouseEnter += gcnew EventHandler(this, &SoftButton::OnHoverEnter);
            this->MouseLeave += gcnew EventHandler(this, &SoftButton::OnHoverLeave);
            this->EnabledChanged += gcnew EventHandler(this, &SoftButton::OnEnabledChanged);
            this->ForeColor = TextColor;
            currentColor = BaseColor;
        }

        // 用于页签激活态切换：currentColor 立即同步到新基色，避免必须靠 hover 触发重绘
        void SetPalette(Color base, Color hover, Color text)
        {
            BaseColor = base;
            HoverColor = hover;
            TextColor = text;
            currentColor = base;
            this->ForeColor = text;
            this->Invalidate();
        }

    protected:
        void OnPaint(PaintEventArgs ^ e) override
        {
            Graphics ^ g = e->Graphics;
            g->SmoothingMode = SmoothingMode::AntiAlias;
            g->TextRenderingHint = ::System::Drawing::Text::TextRenderingHint::ClearTypeGridFit;
            System::Drawing::Rectangle rect(0, 0, this->Width - 1, this->Height - 1);
            GraphicsPath ^ path = RoundedCard::BuildPath(rect, CornerRadius);
            Color fillColor = this->Enabled ? currentColor : Color::FromArgb(0xD8, 0xD4, 0xCD);
            SolidBrush ^ brush = gcnew SolidBrush(fillColor);
            g->FillPath(brush, path);
            delete brush;
            Color tc = this->Enabled ? this->ForeColor : Palette::TextSecondary;
            TextRenderer::DrawText(g, this->Text, this->Font, this->ClientRectangle, tc,
                                   TextFormatFlags::HorizontalCenter | TextFormatFlags::VerticalCenter | TextFormatFlags::NoPadding);
            delete path;
        }
        void OnPaintBackground(PaintEventArgs ^ e) override
        {
            if (this->Parent != nullptr)
            {
                auto state = e->Graphics->Save();
                e->Graphics->TranslateTransform((float)-this->Left, (float)-this->Top);
                System::Drawing::Rectangle parentRect(this->Left, this->Top, this->Width, this->Height);
                PaintEventArgs ^ pe = gcnew PaintEventArgs(e->Graphics, parentRect);
                this->InvokePaintBackground(this->Parent, pe);
                this->InvokePaint(this->Parent, pe);
                e->Graphics->Restore(state);
            }
            // 无父控件时不再 Clear：写入 WindowBg 反而会留下方块，留空更安全
        }

    private:
        Color currentColor;
        void OnHoverEnter(Object ^, EventArgs ^)
        {
            if (this->Enabled)
            {
                currentColor = HoverColor;
                this->Invalidate();
            }
        }
        void OnHoverLeave(Object ^, EventArgs ^)
        {
            if (this->Enabled)
            {
                currentColor = BaseColor;
                this->Invalidate();
            }
        }
        void OnEnabledChanged(Object ^, EventArgs ^)
        {
            currentColor = BaseColor;
            this->Invalidate();
        }
    };

    // 把无边框 TextBox / ComboBox 包进圆角软描边面板，
    // 替代 BorderStyle::FixedSingle 那条在背景图上读起来很硬的 1px 黑线
    ref class RoundedField : public Panel
    {
    public:
        int CornerRadius = 8;
        Color BorderColor;
        Color FillColor;
        int BorderThickness = 1;
        int InnerPadX = 8;
        Control ^ Inner;

        RoundedField(Control ^ inner)
        {
            BorderColor = Palette::CardBorder;
            FillColor = Color::FromArgb(0xFB, 0xF9, 0xF5);
            this->DoubleBuffered = true;
            this->BackColor = Color::Transparent;
            this->SetStyle(ControlStyles::SupportsTransparentBackColor | ControlStyles::AllPaintingInWmPaint | ControlStyles::OptimizedDoubleBuffer | ControlStyles::ResizeRedraw | ControlStyles::UserPaint, true);
            Inner = inner;
            Inner->BackColor = FillColor;
            if (TextBox ^ tb = dynamic_cast<TextBox ^>(Inner))
            {
                // 必须全限定：裸 BorderStyle 在本作用域里会被解析为外层 Panel::BorderStyle 属性
                tb->BorderStyle = System::Windows::Forms::BorderStyle::None;
            }
            this->Controls->Add(Inner);
            this->Resize += gcnew EventHandler(this, &RoundedField::OnSelfResize);
        }

    protected:
        void OnPaint(PaintEventArgs ^ e) override
        {
            Graphics ^ g = e->Graphics;
            g->SmoothingMode = SmoothingMode::AntiAlias;
            System::Drawing::Rectangle rect(0, 0, this->Width - 1, this->Height - 1);
            GraphicsPath ^ path = RoundedCard::BuildPath(rect, CornerRadius);
            SolidBrush ^ fill = gcnew SolidBrush(FillColor);
            g->FillPath(fill, path);
            delete fill;
            Pen ^ pen = gcnew Pen(BorderColor, (float)BorderThickness);
            g->DrawPath(pen, path);
            delete pen;
            delete path;
        }
        void OnPaintBackground(PaintEventArgs ^ e) override
        {
            if (this->Parent != nullptr)
            {
                auto state = e->Graphics->Save();
                e->Graphics->TranslateTransform((float)-this->Left, (float)-this->Top);
                System::Drawing::Rectangle parentRect(this->Left, this->Top, this->Width, this->Height);
                PaintEventArgs ^ pe = gcnew PaintEventArgs(e->Graphics, parentRect);
                this->InvokePaintBackground(this->Parent, pe);
                this->InvokePaint(this->Parent, pe);
                e->Graphics->Restore(state);
            }
            else
            {
                e->Graphics->Clear(Palette::WindowBg);
            }
        }

    private:
        void OnSelfResize(Object ^, EventArgs ^)
        {
            int ph = Inner->PreferredSize.Height;
            if (ph <= 0 || ph > this->Height - 4)
                ph = this->Height - 8;
            Inner->Location = Point(InnerPadX, (this->Height - ph) / 2);
            Inner->Size = Drawing::Size(this->Width - 2 * InnerPadX, ph);
        }
    };

    // 替代系统 ProgressBar：原生 XP 凹陷边框会在背景图上变成一道黑框
    ref class SoftProgressBar : public Control
    {
    public:
        int Minimum = 0;
        int Maximum = 100;
        int CornerRadius = 7;
        Color TrackColor;
        Color FillColor;
        Color BorderColor;

        SoftProgressBar()
        {
            TrackColor = Color::FromArgb(0xE4, 0xE1, 0xDB);
            FillColor = Palette::Primary;
            BorderColor = Palette::CardBorder;
            // SetStyle 必须先于 BackColor=Transparent；Control (与 Panel 不同) 在没有该标志时会拒绝透明背景
            this->SetStyle(ControlStyles::SupportsTransparentBackColor | ControlStyles::AllPaintingInWmPaint | ControlStyles::OptimizedDoubleBuffer | ControlStyles::ResizeRedraw | ControlStyles::UserPaint, true);
            this->DoubleBuffered = true;
            this->BackColor = Color::Transparent;
        }

        property int Value
        {
            int get() { return _value; }
            void set(int v)
            {
                if (v < Minimum)
                    v = Minimum;
                if (v > Maximum)
                    v = Maximum;
                _value = v;
                this->Invalidate();
            }
        }

    protected:
        void OnPaint(PaintEventArgs ^ e) override
        {
            Graphics ^ g = e->Graphics;
            g->SmoothingMode = SmoothingMode::AntiAlias;
            System::Drawing::Rectangle r(0, 0, this->Width - 1, this->Height - 1);
            GraphicsPath ^ trackPath = RoundedCard::BuildPath(r, CornerRadius);
            SolidBrush ^ trackBrush = gcnew SolidBrush(TrackColor);
            g->FillPath(trackBrush, trackPath);
            delete trackBrush;
            int range = Maximum - Minimum;
            if (range > 0 && _value > Minimum)
            {
                int fillW = (int)((float)(this->Width - 1) * (float)(_value - Minimum) / (float)range);
                if (fillW >= CornerRadius * 2)
                {
                    System::Drawing::Rectangle fr(0, 0, fillW, this->Height - 1);
                    GraphicsPath ^ fillPath = RoundedCard::BuildPath(fr, CornerRadius);
                    SolidBrush ^ fillBrush = gcnew SolidBrush(FillColor);
                    g->FillPath(fillBrush, fillPath);
                    delete fillBrush;
                    delete fillPath;
                }
            }
            Pen ^ pen = gcnew Pen(BorderColor, 1.0f);
            g->DrawPath(pen, trackPath);
            delete pen;
            delete trackPath;
        }
        void OnPaintBackground(PaintEventArgs ^ e) override
        {
            if (this->Parent != nullptr)
            {
                auto state = e->Graphics->Save();
                e->Graphics->TranslateTransform((float)-this->Left, (float)-this->Top);
                System::Drawing::Rectangle parentRect(this->Left, this->Top, this->Width, this->Height);
                PaintEventArgs ^ pe = gcnew PaintEventArgs(e->Graphics, parentRect);
                this->InvokePaintBackground(this->Parent, pe);
                this->InvokePaint(this->Parent, pe);
                e->Graphics->Restore(state);
            }
        }

    private:
        int _value;
    };

    // 与 sp_progress_cb 签名一致的 Cdecl 委托，用于把 native 进度回调
    // 转给 BackgroundWorker::ReportProgress 走到 UI 线程
    [UnmanagedFunctionPointer(System::Runtime::InteropServices::CallingConvention::Cdecl)] public delegate void NativeProgressDelegate(uint32_t current, uint32_t total,
                                                                                                                                       const char *name_utf8, void *user);

public
    ref class MainForm : public Form
    {
    public:
        MainForm()
        {
            InitializeComponent();
            InitializeIcon();
            RefreshPackDefaults();
            SetLastOpenTarget(GetDefaultUnpackFullPath());
            SetIdleState(L"待机中  ❖", L"将 .pac 拖入此处开始解包。或切换到 Pack 页签进行反汇编 / 重建 / 解密。");
            StartFadeIn();
        }

    protected:
        ~MainForm() override {}

    private:
        TransparentPanel ^ headerPanel;
        PictureBox ^ logoBox;
        Label ^ brandTitleLabel;
        Label ^ brandSubtitleLabel;

        SoftButton ^ extractTabButton;
        SoftButton ^ packTabButton;
        TransparentPanel ^ extractPanel;
        TransparentPanel ^ packPanel;

        RoundedCard ^ dropCard;
        Label ^ dropTitleLabel;
        Label ^ dropHintLabel;
        Label ^ outputRootLabel;
        Label ^ extractInfoLabel;

        Label ^ packIntroLabel;
        SoftButton ^ detectDefaultsButton;

        RoundedCard ^ disasmCard;
        Label ^ disasmCardTitle;
        TextBox ^ scriptInTextBox;
        TextBox ^ textInTextBox;
        TextBox ^ jsonOutTextBox;
        SoftButton ^ browseScriptInButton;
        SoftButton ^ browseTextInButton;
        SoftButton ^ browseJsonOutButton;
        SoftButton ^ disassembleButton;

        RoundedCard ^ rebuildCard;
        Label ^ rebuildCardTitle;
        TextBox ^ rbJsonTextBox;
        TextBox ^ rbOutDirTextBox;
        ComboBox ^ rbEncodingComboBox;
        SoftButton ^ browseRbJsonButton;
        SoftButton ^ browseRbOutDirButton;
        SoftButton ^ rebuildButton;

        RoundedCard ^ palCard;
        Label ^ palCardTitle;
        TextBox ^ palInTextBox;
        TextBox ^ palOutTextBox;
        SoftButton ^ browsePalInButton;
        SoftButton ^ browsePalOutButton;
        SoftButton ^ palDecryptButton;

        Label ^ statusLabel;
        SoftProgressBar ^ progressBar;
        Label ^ currentLogTitleLabel;
        TextBox ^ currentLogBox;
        RoundedField ^ currentLogField;
        SoftButton ^ openResultButton;
        SoftButton ^ resetStateButton;

        Timer ^ fadeTimer;
        BackgroundWorker ^ worker;

        String ^ busyStatusPrefix = String::Empty;
        String ^ lastOpenTarget = String::Empty;

        // 必须在 form 上保持强引用：Marshal::GetFunctionPointerForDelegate
        // 把它转成 sp_progress_cb，但底层不会延长托管对象寿命，被 GC 后回调悬空
        NativeProgressDelegate ^ progressDelegate;
        sp_progress_cb progressCallback;
        String ^ phaseLabel = String::Empty;

        void StyleTextBox(TextBox ^ b)
        {
            b->BorderStyle = BorderStyle::None;
            b->BackColor = Color::FromArgb(0xFB, 0xF9, 0xF5);
            b->ForeColor = Palette::TextPrimary;
            b->Font = gcnew ::System::Drawing::Font(L"Consolas", 9.5f, FontStyle::Regular);
        }
        void StyleComboBox(ComboBox ^ b)
        {
            b->FlatStyle = System::Windows::Forms::FlatStyle::Flat;
            b->BackColor = Color::FromArgb(0xFB, 0xF9, 0xF5);
            b->ForeColor = Palette::TextPrimary;
            b->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
        }
        // 调用方应把返回的 panel 加入父容器，而不是直接添加 inner
        RoundedField ^ WrapField(Control ^ inner, int x, int y, int w, int h)
        {
            RoundedField ^ panel = gcnew RoundedField(inner);
            panel->Location = Point(x, y);
            panel->Size = Drawing::Size(w, h);
            return panel;
        }
        Label ^ MakeFieldLabel(String ^ text, int x, int y)
        {
            Label ^ l = gcnew Label();
            l->AutoSize = true;
            l->BackColor = Color::Transparent;
            l->ForeColor = Palette::TextSecondary;
            l->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.0f, FontStyle::Regular);
            l->Location = Point(x, y);
            l->Text = text;
            return l;
        }
        Label ^ MakeCardTitle(String ^ text)
        {
            Label ^ l = gcnew Label();
            l->AutoSize = true;
            l->BackColor = Color::Transparent;
            l->ForeColor = Palette::Accent;
            l->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 11.0f, FontStyle::Bold);
            l->Text = text;
            return l;
        }
        SoftButton ^ MakePrimaryButton(String ^ text)
        {
            SoftButton ^ b = gcnew SoftButton();
            b->SetPalette(Palette::Primary, Palette::PrimaryHover, Palette::PrimaryText);
            b->Text = text;
            return b;
        }
        SoftButton ^ MakeSecondaryButton(String ^ text)
        {
            SoftButton ^ b = gcnew SoftButton();
            b->SetPalette(Palette::Secondary, Palette::SecondaryHover, Palette::PrimaryText);
            b->Text = text;
            return b;
        }
        SoftButton ^ MakeGhostButton(String ^ text)
        {
            SoftButton ^ b = gcnew SoftButton();
            b->SetPalette(Palette::Ghost, Palette::GhostHover, Palette::GhostText);
            b->CornerRadius = 8;
            b->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
            b->Text = text;
            return b;
        }

        Bitmap ^ formBackdrop;

        void LoadBackgroundImage()
        {
            try
            {
                HMODULE hMod = ::GetModuleHandleW(nullptr);
                HRSRC hRes = ::FindResourceW(hMod, MAKEINTRESOURCEW(201), RT_RCDATA);
                if (!hRes)
                    return;
                HGLOBAL hGlob = ::LoadResource(hMod, hRes);
                if (!hGlob)
                    return;
                DWORD bytesLen = ::SizeofResource(hMod, hRes);
                void *bytesPtr = ::LockResource(hGlob);
                if (!bytesPtr || bytesLen == 0)
                    return;
                array<Byte> ^ buffer = gcnew array<Byte>((int)bytesLen);
                Marshal::Copy(IntPtr(bytesPtr), buffer, 0, (int)bytesLen);
                MemoryStream ^ stream = gcnew MemoryStream(buffer);
                Image ^ source = Image::FromStream(stream);

                int clientW = this->ClientSize.Width;
                int clientH = this->ClientSize.Height;
                formBackdrop = gcnew Bitmap(clientW, clientH);
                Graphics ^ g = Graphics::FromImage(formBackdrop);
                g->InterpolationMode = InterpolationMode::HighQualityBicubic;
                g->SmoothingMode = SmoothingMode::AntiAlias;
                g->Clear(Palette::WindowBg);

                // cover 缩放后向右上偏移：让人物面部落在卡片密集的下半部之外的留白区
                float scale = System::Math::Max((float)clientW / source->Width,
                                                (float)clientH / source->Height);
                int dw = (int)(source->Width * scale);
                int dh = (int)(source->Height * scale);
                int offsetX = (clientW - dw) / 2 + 110;
                int offsetY = (clientH - dh) / 2 - 90;
                g->DrawImage(source, offsetX, offsetY, dw, dh);

                // 仅施加极薄一层米白蒙版以维持文字可读性，主视觉仍以背景图为主
                SolidBrush ^ veil = gcnew SolidBrush(Color::FromArgb(55, 0xFA, 0xF6, 0xF0));
                g->FillRectangle(veil, 0, 0, clientW, clientH);
                delete veil;
                delete g;
                delete source;
            }
            catch (Exception ^)
            {
                formBackdrop = nullptr;
            }
        }

        void OnFormPaint(Object ^, PaintEventArgs ^ e)
        {
            Graphics ^ g = e->Graphics;
            if (formBackdrop != nullptr)
            {
                g->DrawImage(formBackdrop, 0, 0, this->ClientSize.Width, this->ClientSize.Height);
            }
            g->SmoothingMode = SmoothingMode::AntiAlias;
            // 顶部带：上浓下淡的米白渐变，既给标题足够对比度又让背景在 tab 附近露出
            System::Drawing::Rectangle headerRect(0, 0, this->ClientSize.Width, 76);
            LinearGradientBrush ^ headerBrush = gcnew LinearGradientBrush(
                headerRect,
                Color::FromArgb(170, 0xFA, 0xF6, 0xF0),
                Color::FromArgb(70, 0xFA, 0xF6, 0xF0),
                LinearGradientMode::Vertical);
            g->FillRectangle(headerBrush, headerRect);
            delete headerBrush;
            // 底部带：渐变方向反转，最浓的部分压在状态条 / 进度条 / 日志框下方
            int footerTop = 558;
            System::Drawing::Rectangle footerRect(0, footerTop, this->ClientSize.Width,
                                                  this->ClientSize.Height - footerTop);
            LinearGradientBrush ^ footerBrush = gcnew LinearGradientBrush(
                footerRect,
                Color::FromArgb(60, 0xFA, 0xF6, 0xF0),
                Color::FromArgb(195, 0xFA, 0xF6, 0xF0),
                LinearGradientMode::Vertical);
            g->FillRectangle(footerBrush, footerRect);
            delete footerBrush;
            // 在两条带的边缘补一条藏青发丝线，区隔带与艺术稿区域
            Pen ^ rule = gcnew Pen(Color::FromArgb(70, Palette::Primary), 1.0f);
            g->DrawLine(rule, 0, 76, this->ClientSize.Width, 76);
            g->DrawLine(rule, 0, footerTop, this->ClientSize.Width, footerTop);
            delete rule;
        }

        Bitmap ^ LoadEmbeddedPng(int rcId)
        {
            try
            {
                HMODULE hMod = ::GetModuleHandleW(nullptr);
                HRSRC hRes = ::FindResourceW(hMod, MAKEINTRESOURCEW(rcId), RT_RCDATA);
                if (!hRes)
                    return nullptr;
                HGLOBAL hGlob = ::LoadResource(hMod, hRes);
                if (!hGlob)
                    return nullptr;
                DWORD bytesLen = ::SizeofResource(hMod, hRes);
                void *bytesPtr = ::LockResource(hGlob);
                if (!bytesPtr || bytesLen == 0)
                    return nullptr;
                array<Byte> ^ buffer = gcnew array<Byte>((int)bytesLen);
                Marshal::Copy(IntPtr(bytesPtr), buffer, 0, (int)bytesLen);
                MemoryStream ^ stream = gcnew MemoryStream(buffer);
                return gcnew Bitmap(stream);
            }
            catch (Exception ^)
            {
                return nullptr;
            }
        }

        void InitializeIcon()
        {
            // 窗口 / 任务栏图标走 ICON 资源（icon.ico）
            try
            {
                String ^ exePath = Assembly::GetExecutingAssembly()->Location;
                System::Drawing::Icon ^ ico = System::Drawing::Icon::ExtractAssociatedIcon(exePath);
                if (ico != nullptr)
                    this->Icon = ico;
            }
            catch (Exception ^)
            {
            }

            // 页眉 LOGO 改用 RCDATA 202 的高清 PNG，避免 ExtractAssociatedIcon 拿到的 16x16 缩略图
            try
            {
                Bitmap ^ src = LoadEmbeddedPng(202);
                if (src != nullptr)
                {
                    Bitmap ^ bmp = gcnew Bitmap(48, 48);
                    Graphics ^ g = Graphics::FromImage(bmp);
                    g->SmoothingMode = SmoothingMode::AntiAlias;
                    g->InterpolationMode = InterpolationMode::HighQualityBicubic;
                    g->PixelOffsetMode = ::System::Drawing::Drawing2D::PixelOffsetMode::HighQuality;
                    g->DrawImage(src, System::Drawing::Rectangle(0, 0, 48, 48));
                    delete g;
                    delete src;
                    logoBox->Image = bmp;
                    logoBox->SizeMode = PictureBoxSizeMode::Zoom;
                }
            }
            catch (Exception ^)
            {
            }
        }

        void StartFadeIn()
        {
            this->Opacity = 0.0;
            fadeTimer = gcnew Timer();
            fadeTimer->Interval = 15;
            fadeTimer->Tick += gcnew EventHandler(this, &MainForm::OnFadeTick);
            fadeTimer->Start();
        }
        void OnFadeTick(Object ^, EventArgs ^)
        {
            double next = this->Opacity + 0.06;
            if (next >= 1.0)
            {
                this->Opacity = 1.0;
                fadeTimer->Stop();
                return;
            }
            this->Opacity = next;
        }

        void InitializeComponent()
        {
            SuspendLayout();
            AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
            AllowDrop = false;
            BackColor = Palette::WindowBg;
            ClientSize = Drawing::Size(920, 700);
            FormBorderStyle = System::Windows::Forms::FormBorderStyle::FixedSingle;
            MaximizeBox = false;
            MinimizeBox = true;
            StartPosition = FormStartPosition::CenterScreen;
            Text = L"SoftPal-Sakura  ❖  SoftPal 引擎 解封包工具";
            Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
            DoubleBuffered = true;

            LoadBackgroundImage();
            this->Paint += gcnew PaintEventHandler(this, &MainForm::OnFormPaint);

            BuildHeader();
            BuildTabs();
            BuildStatusBar();

            Controls->Add(headerPanel);
            Controls->Add(extractTabButton);
            Controls->Add(packTabButton);
            Controls->Add(extractPanel);
            Controls->Add(packPanel);
            Controls->Add(statusLabel);
            Controls->Add(openResultButton);
            Controls->Add(resetStateButton);
            Controls->Add(progressBar);
            Controls->Add(currentLogTitleLabel);
            Controls->Add(currentLogField);

            worker = gcnew BackgroundWorker();
            worker->WorkerReportsProgress = true;
            worker->DoWork += gcnew DoWorkEventHandler(this, &MainForm::OnWorkerDoWork);
            worker->RunWorkerCompleted += gcnew RunWorkerCompletedEventHandler(this, &MainForm::OnWorkerCompleted);
            worker->ProgressChanged += gcnew ProgressChangedEventHandler(this, &MainForm::OnWorkerProgressChanged);

            progressDelegate = gcnew NativeProgressDelegate(this, &MainForm::OnNativeProgress);
            IntPtr fp = Marshal::GetFunctionPointerForDelegate(progressDelegate);
            progressCallback = static_cast<sp_progress_cb>(fp.ToPointer());

            ResumeLayout(false);
        }

        // 在 worker 线程上由 native 侧回调；通过 BackgroundWorker 转回 UI 线程
        void OnNativeProgress(uint32_t current, uint32_t total, const char *name_utf8, void *user)
        {
            if (worker == nullptr)
                return;
            int pct = 0;
            if (total > 0)
            {
                unsigned long long v = (unsigned long long)current * 100ull / total;
                if (v > 100)
                    v = 100;
                pct = (int)v;
            }
            // PAC 条目名是纯 ASCII，PtrToStringAnsi 即可（避免引入 UTF-8 解码依赖）
            String ^ name = String::Empty;
            if (name_utf8 != nullptr)
            {
                name = Marshal::PtrToStringAnsi(IntPtr((void *)name_utf8));
                if (name == nullptr)
                    name = String::Empty;
            }
            worker->ReportProgress(pct, name);
        }

        void OnWorkerProgressChanged(Object ^, ProgressChangedEventArgs ^ e)
        {
            int v = System::Math::Max(0, System::Math::Min(100, e->ProgressPercentage));
            progressBar->Value = v;
            statusLabel->Text = String::IsNullOrEmpty(busyStatusPrefix)
                                    ? (v.ToString() + L"%")
                                    : (busyStatusPrefix + L" " + v.ToString() + L"%");
            String ^ name = (e->UserState == nullptr) ? String::Empty : safe_cast<String ^>(e->UserState);
            String ^ line = String::IsNullOrEmpty(phaseLabel)
                                ? name
                                : (String::IsNullOrEmpty(name) ? phaseLabel : (phaseLabel + L"  ·  " + name));
            if (!String::IsNullOrEmpty(line))
            {
                currentLogBox->Text = line;
                currentLogBox->SelectionStart = 0;
                currentLogBox->SelectionLength = 0;
            }
        }

        void BuildHeader()
        {
            headerPanel = gcnew TransparentPanel();
            headerPanel->Location = Point(0, 0);
            headerPanel->Size = Drawing::Size(920, 76);

            logoBox = gcnew PictureBox();
            logoBox->Location = Point(22, 14);
            logoBox->Size = Drawing::Size(48, 48);
            logoBox->BackColor = Color::Transparent;

            brandTitleLabel = gcnew Label();
            brandTitleLabel->AutoSize = false;
            brandTitleLabel->Location = Point(82, 12);
            brandTitleLabel->Size = Drawing::Size(700, 30);
            brandTitleLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 16.0f, FontStyle::Bold);
            brandTitleLabel->ForeColor = Palette::Accent;
            brandTitleLabel->BackColor = Color::Transparent;
            brandTitleLabel->Text = L"SoftPal-Sakura  ❖  SoftPal 引擎 解封包工具";

            brandSubtitleLabel = gcnew Label();
            brandSubtitleLabel->AutoSize = false;
            brandSubtitleLabel->Location = Point(84, 44);
            brandSubtitleLabel->Size = Drawing::Size(820, 22);
            brandSubtitleLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
            brandSubtitleLabel->ForeColor = Palette::TextSecondary;
            brandSubtitleLabel->BackColor = Color::Transparent;
            brandSubtitleLabel->Text = L"拖拽 .pac 即可解包  ·  Pack 页签提供反汇编 / 重建 / 解密";

            headerPanel->Controls->Add(logoBox);
            headerPanel->Controls->Add(brandTitleLabel);
            headerPanel->Controls->Add(brandSubtitleLabel);
        }

        void OnExtractTabClicked(Object ^, EventArgs ^)
        {
            extractTabButton->SetPalette(Palette::Primary, Palette::PrimaryHover, Palette::PrimaryText);
            packTabButton->SetPalette(Palette::Ghost, Palette::GhostHover, Palette::GhostText);
            extractPanel->Visible = true;
            packPanel->Visible = false;
        }

        void OnPackTabClicked(Object ^, EventArgs ^)
        {
            packTabButton->SetPalette(Palette::Primary, Palette::PrimaryHover, Palette::PrimaryText);
            extractTabButton->SetPalette(Palette::Ghost, Palette::GhostHover, Palette::GhostText);
            packPanel->Visible = true;
            extractPanel->Visible = false;
        }

        void BuildTabs()
        {
            extractTabButton = MakeGhostButton(L"  ❖  解包 Extract  ");
            extractTabButton->Location = Point(18, 86);
            extractTabButton->Size = Drawing::Size(150, 42);
            extractTabButton->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.0f, FontStyle::Bold);
            extractTabButton->Click += gcnew EventHandler(this, &MainForm::OnExtractTabClicked);

            packTabButton = MakeGhostButton(L"  ◆  封包 Pack  ");
            packTabButton->Location = Point(176, 86);
            packTabButton->Size = Drawing::Size(150, 42);
            packTabButton->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.0f, FontStyle::Bold);
            packTabButton->Click += gcnew EventHandler(this, &MainForm::OnPackTabClicked);

            extractPanel = gcnew TransparentPanel();
            extractPanel->Location = Point(18, 136);
            extractPanel->Size = Drawing::Size(884, 422);
            extractPanel->AllowDrop = true;
            extractPanel->DragEnter += gcnew DragEventHandler(this, &MainForm::OnDragEnter);
            extractPanel->DragDrop += gcnew DragEventHandler(this, &MainForm::OnDragDrop);

            packPanel = gcnew TransparentPanel();
            packPanel->Location = Point(18, 136);
            packPanel->Size = Drawing::Size(884, 422);
            packPanel->Visible = false;

            BuildExtractTab();
            BuildPackTab();

            OnExtractTabClicked(nullptr, nullptr);
        }

        void BuildExtractTab()
        {
            dropCard = gcnew RoundedCard();
            dropCard->FillColor = Color::FromArgb(95, 0xFA, 0xF6, 0xF0);
            dropCard->BorderColor = Palette::DropBorder;
            dropCard->BorderThickness = 2;
            dropCard->CornerRadius = 22;
            dropCard->Location = Point(22, 20);
            dropCard->Size = Drawing::Size(836, 270);
            dropCard->AllowDrop = true;
            dropCard->DragEnter += gcnew DragEventHandler(this, &MainForm::OnDragEnter);
            dropCard->DragDrop += gcnew DragEventHandler(this, &MainForm::OnDragDrop);

            dropTitleLabel = gcnew Label();
            dropTitleLabel->AutoSize = false;
            dropTitleLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 20.0f, FontStyle::Bold);
            dropTitleLabel->ForeColor = Palette::Accent;
            dropTitleLabel->BackColor = Color::Transparent;
            dropTitleLabel->Location = Point(22, 56);
            dropTitleLabel->Size = Drawing::Size(790, 42);
            dropTitleLabel->Text = L"❖   将 .pac 拖到这里   ❖";
            dropTitleLabel->TextAlign = ContentAlignment::MiddleCenter;

            dropHintLabel = gcnew Label();
            dropHintLabel->AutoSize = false;
            dropHintLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.5f, FontStyle::Regular);
            dropHintLabel->ForeColor = Palette::TextSecondary;
            dropHintLabel->BackColor = Color::Transparent;
            dropHintLabel->Location = Point(60, 112);
            dropHintLabel->Size = Drawing::Size(716, 56);
            dropHintLabel->Text = L"程序会自动识别归档\r\n.pac  →  unpack \\ <归档名> \\";
            dropHintLabel->TextAlign = ContentAlignment::MiddleCenter;

            outputRootLabel = gcnew Label();
            outputRootLabel->AutoSize = false;
            outputRootLabel->Font = gcnew ::System::Drawing::Font(L"Consolas", 10.0f, FontStyle::Regular);
            outputRootLabel->ForeColor = Palette::TextPrimary;
            outputRootLabel->BackColor = Color::Transparent;
            outputRootLabel->Location = Point(24, 196);
            outputRootLabel->Size = Drawing::Size(788, 22);
            outputRootLabel->Text = L"输出目录: " + GetDefaultUnpackFullPath();
            outputRootLabel->TextAlign = ContentAlignment::MiddleCenter;

            dropCard->Controls->Add(dropTitleLabel);
            dropCard->Controls->Add(dropHintLabel);
            dropCard->Controls->Add(outputRootLabel);

            extractInfoLabel = gcnew Label();
            extractInfoLabel->AutoSize = false;
            extractInfoLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
            extractInfoLabel->ForeColor = Palette::TextSecondary;
            extractInfoLabel->BackColor = Color::Transparent;
            extractInfoLabel->Location = Point(22, 300);
            extractInfoLabel->Size = Drawing::Size(836, 96);
            extractInfoLabel->Text =
                L"使用提示\r\n"
                L"·  先在此处解包，再切换到 Pack 页签进行反汇编、重建或 PAL 解密\r\n"
                L"·  使用 exe + dll 时，请将其放置在游戏根目录同层";

            extractPanel->Controls->Add(dropCard);
            extractPanel->Controls->Add(extractInfoLabel);
        }

        void BuildPackTab()
        {
            packIntroLabel = gcnew Label();
            packIntroLabel->AutoSize = false;
            packIntroLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Regular);
            packIntroLabel->ForeColor = Palette::TextSecondary;
            packIntroLabel->BackColor = Color::Transparent;
            packIntroLabel->Location = Point(22, 6);
            packIntroLabel->Size = Drawing::Size(700, 40);
            packIntroLabel->Text =
                L"流程：解包 → 反汇编 JSON → 修改翻译 → 重建写回\r\n"
                L"PAL 解密用于较老版本脚本，新版多为明文";

            detectDefaultsButton = MakeGhostButton(L"刷新默认路径");
            detectDefaultsButton->Location = Point(730, 12);
            detectDefaultsButton->Size = Drawing::Size(128, 28);
            detectDefaultsButton->Click += gcnew EventHandler(this, &MainForm::OnRefreshDefaultsClicked);

            BuildDisasmCard();
            BuildRebuildCard();
            BuildPalCard();

            packPanel->Controls->Add(packIntroLabel);
            packPanel->Controls->Add(detectDefaultsButton);
            packPanel->Controls->Add(disasmCard);
            packPanel->Controls->Add(rebuildCard);
            packPanel->Controls->Add(palCard);
        }

        void BuildDisasmCard()
        {
            disasmCard = gcnew RoundedCard();
            disasmCard->FillColor = Color::FromArgb(170, 0xFA, 0xF6, 0xF0);
            disasmCard->Location = Point(22, 50);
            disasmCard->Size = Drawing::Size(836, 170);

            disasmCardTitle = MakeCardTitle(L"❖  脚本反汇编  →  JSON");
            disasmCardTitle->Location = Point(20, 14);

            scriptInTextBox = gcnew TextBox();
            StyleTextBox(scriptInTextBox);
            RoundedField ^ scriptInField = WrapField(scriptInTextBox, 98, 44, 588, 28);

            browseScriptInButton = MakeGhostButton(L"浏览");
            browseScriptInButton->Location = Point(696, 44);
            browseScriptInButton->Size = Drawing::Size(68, 28);
            browseScriptInButton->Click += gcnew EventHandler(this, &MainForm::OnBrowseScriptInClicked);

            textInTextBox = gcnew TextBox();
            StyleTextBox(textInTextBox);
            RoundedField ^ textInField = WrapField(textInTextBox, 98, 76, 588, 28);

            browseTextInButton = MakeGhostButton(L"浏览");
            browseTextInButton->Location = Point(696, 76);
            browseTextInButton->Size = Drawing::Size(68, 28);
            browseTextInButton->Click += gcnew EventHandler(this, &MainForm::OnBrowseTextInClicked);

            jsonOutTextBox = gcnew TextBox();
            StyleTextBox(jsonOutTextBox);
            RoundedField ^ jsonOutField = WrapField(jsonOutTextBox, 98, 108, 588, 28);

            browseJsonOutButton = MakeGhostButton(L"另存");
            browseJsonOutButton->Location = Point(696, 108);
            browseJsonOutButton->Size = Drawing::Size(68, 28);
            browseJsonOutButton->Click += gcnew EventHandler(this, &MainForm::OnBrowseJsonOutClicked);

            disassembleButton = MakePrimaryButton(L"导出 JSON");
            disassembleButton->Location = Point(624, 136);
            disassembleButton->Size = Drawing::Size(140, 32);
            disassembleButton->Click += gcnew EventHandler(this, &MainForm::OnDisassembleClicked);

            disasmCard->Controls->Add(disasmCardTitle);
            disasmCard->Controls->Add(MakeFieldLabel(L"SCRIPT.SRC", 22, 50));
            disasmCard->Controls->Add(scriptInField);
            disasmCard->Controls->Add(browseScriptInButton);
            disasmCard->Controls->Add(MakeFieldLabel(L"TEXT.DAT", 22, 82));
            disasmCard->Controls->Add(textInField);
            disasmCard->Controls->Add(browseTextInButton);
            disasmCard->Controls->Add(MakeFieldLabel(L"输出 JSON", 22, 114));
            disasmCard->Controls->Add(jsonOutField);
            disasmCard->Controls->Add(browseJsonOutButton);
            disasmCard->Controls->Add(disassembleButton);
        }

        void BuildRebuildCard()
        {
            rebuildCard = gcnew RoundedCard();
            rebuildCard->FillColor = Color::FromArgb(170, 0xFA, 0xF6, 0xF0);
            rebuildCard->Location = Point(22, 226);
            rebuildCard->Size = Drawing::Size(836, 108);

            rebuildCardTitle = MakeCardTitle(L"❖  脚本重建  (JSON → SCRIPT.SRC + TEXT.DAT)");
            rebuildCardTitle->Location = Point(20, 14);

            rbJsonTextBox = gcnew TextBox();
            StyleTextBox(rbJsonTextBox);
            RoundedField ^ rbJsonField = WrapField(rbJsonTextBox, 98, 42, 588, 28);

            browseRbJsonButton = MakeGhostButton(L"浏览");
            browseRbJsonButton->Location = Point(696, 42);
            browseRbJsonButton->Size = Drawing::Size(68, 28);
            browseRbJsonButton->Click += gcnew EventHandler(this, &MainForm::OnBrowseRbJsonClicked);

            rbOutDirTextBox = gcnew TextBox();
            StyleTextBox(rbOutDirTextBox);
            RoundedField ^ rbOutDirField = WrapField(rbOutDirTextBox, 98, 74, 420, 28);

            browseRbOutDirButton = MakeGhostButton(L"浏览");
            browseRbOutDirButton->Location = Point(528, 74);
            browseRbOutDirButton->Size = Drawing::Size(58, 28);
            browseRbOutDirButton->Click += gcnew EventHandler(this, &MainForm::OnBrowseRbOutDirClicked);

            rbEncodingComboBox = gcnew ComboBox();
            StyleComboBox(rbEncodingComboBox);
            rbEncodingComboBox->DropDownStyle = ComboBoxStyle::DropDownList;
            rbEncodingComboBox->Items->AddRange(gcnew array<Object ^>{L"gbk", L"shift_jis"});
            rbEncodingComboBox->SelectedIndex = 0;
            RoundedField ^ rbEncodingField = WrapField(rbEncodingComboBox, 600, 74, 96, 28);
            rbEncodingField->InnerPadX = 4;

            rebuildButton = MakePrimaryButton(L"重建");
            rebuildButton->Location = Point(708, 74);
            rebuildButton->Size = Drawing::Size(110, 28);
            rebuildButton->Click += gcnew EventHandler(this, &MainForm::OnRebuildClicked);

            rebuildCard->Controls->Add(rebuildCardTitle);
            rebuildCard->Controls->Add(MakeFieldLabel(L"翻译 JSON", 22, 48));
            rebuildCard->Controls->Add(rbJsonField);
            rebuildCard->Controls->Add(browseRbJsonButton);
            rebuildCard->Controls->Add(MakeFieldLabel(L"输出目录", 22, 80));
            rebuildCard->Controls->Add(rbOutDirField);
            rebuildCard->Controls->Add(browseRbOutDirButton);
            rebuildCard->Controls->Add(rbEncodingField);
            rebuildCard->Controls->Add(rebuildButton);
        }

        void BuildPalCard()
        {
            palCard = gcnew RoundedCard();
            palCard->FillColor = Color::FromArgb(170, 0xFA, 0xF6, 0xF0);
            palCard->Location = Point(22, 340);
            palCard->Size = Drawing::Size(836, 80);

            palCardTitle = MakeCardTitle(L"❖  PAL 解密  (ROL + XOR)");
            palCardTitle->Location = Point(20, 14);

            palInTextBox = gcnew TextBox();
            StyleTextBox(palInTextBox);
            RoundedField ^ palInField = WrapField(palInTextBox, 82, 38, 248, 28);

            browsePalInButton = MakeGhostButton(L"浏览");
            browsePalInButton->Location = Point(334, 38);
            browsePalInButton->Size = Drawing::Size(50, 28);
            browsePalInButton->Click += gcnew EventHandler(this, &MainForm::OnBrowsePalInClicked);

            palOutTextBox = gcnew TextBox();
            StyleTextBox(palOutTextBox);
            RoundedField ^ palOutField = WrapField(palOutTextBox, 434, 38, 202, 28);

            browsePalOutButton = MakeGhostButton(L"另存");
            browsePalOutButton->Location = Point(640, 38);
            browsePalOutButton->Size = Drawing::Size(50, 28);
            browsePalOutButton->Click += gcnew EventHandler(this, &MainForm::OnBrowsePalOutClicked);

            palDecryptButton = MakeSecondaryButton(L"解密");
            palDecryptButton->Location = Point(708, 38);
            palDecryptButton->Size = Drawing::Size(110, 28);
            palDecryptButton->Click += gcnew EventHandler(this, &MainForm::OnPalDecryptClicked);

            palCard->Controls->Add(palCardTitle);
            palCard->Controls->Add(MakeFieldLabel(L"输入", 22, 44));
            palCard->Controls->Add(palInField);
            palCard->Controls->Add(browsePalInButton);
            palCard->Controls->Add(MakeFieldLabel(L"输出", 396, 44));
            palCard->Controls->Add(palOutField);
            palCard->Controls->Add(browsePalOutButton);
            palCard->Controls->Add(palDecryptButton);
        }

        void BuildStatusBar()
        {
            statusLabel = gcnew Label();
            statusLabel->AutoSize = false;
            statusLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.0f, FontStyle::Regular);
            statusLabel->ForeColor = Palette::TextPrimary;
            statusLabel->BackColor = Color::Transparent;
            statusLabel->Location = Point(22, 572);
            statusLabel->Size = Drawing::Size(580, 24);

            openResultButton = MakePrimaryButton(L"打开结果");
            openResultButton->Location = Point(680, 568);
            openResultButton->Size = Drawing::Size(108, 32);
            openResultButton->Click += gcnew EventHandler(this, &MainForm::OnOpenResultClicked);

            resetStateButton = MakeGhostButton(L"重置状态");
            resetStateButton->Location = Point(794, 568);
            resetStateButton->Size = Drawing::Size(108, 32);
            resetStateButton->Click += gcnew EventHandler(this, &MainForm::OnResetStateClicked);

            progressBar = gcnew SoftProgressBar();
            progressBar->Location = Point(22, 608);
            progressBar->Size = Drawing::Size(880, 16);

            currentLogTitleLabel = gcnew Label();
            currentLogTitleLabel->AutoSize = false;
            currentLogTitleLabel->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 9.5f, FontStyle::Bold);
            currentLogTitleLabel->ForeColor = Palette::Accent;
            currentLogTitleLabel->BackColor = Color::Transparent;
            currentLogTitleLabel->Location = Point(22, 636);
            currentLogTitleLabel->Size = Drawing::Size(120, 28);
            currentLogTitleLabel->TextAlign = ContentAlignment::MiddleLeft;
            currentLogTitleLabel->Text = L"当前步骤  ❖";

            currentLogBox = gcnew TextBox();
            currentLogBox->BackColor = Color::FromArgb(0xFB, 0xF9, 0xF5);
            currentLogBox->ForeColor = Palette::TextPrimary;
            currentLogBox->Font = gcnew ::System::Drawing::Font(L"Microsoft YaHei UI", 10.0f, FontStyle::Regular);
            currentLogBox->ReadOnly = true;
            currentLogBox->ShortcutsEnabled = false;
            currentLogField = WrapField(currentLogBox, 146, 636, 756, 28);
        }

        String ^ GetWorkspaceRoot() { return Directory::GetCurrentDirectory(); }

        String ^ GetDefaultUnpackFullPath()
        {
            return Path::GetFullPath(Path::Combine(GetWorkspaceRoot(), L"unpack"));
        }

        String ^ GetDefaultUnpackDirForPac(String ^ pacPath)
        {
            String ^ stem = Path::GetFileNameWithoutExtension(pacPath);
            return Path::Combine(GetDefaultUnpackFullPath(), stem);
        }

        String ^ ResolveOptionalPath(String ^ value)
        {
            if (String::IsNullOrWhiteSpace(value))
                return String::Empty;
            return Path::GetFullPath(value);
        }

        void RefreshPackDefaults()
        {
            if (rbEncodingComboBox != nullptr && rbEncodingComboBox->SelectedIndex < 0)
            {
                rbEncodingComboBox->SelectedIndex = 0;
            }
        }

        void SetCurrentLog(String ^ message)
        {
            currentLogBox->Text = String::IsNullOrWhiteSpace(message) ? L"等待任务开始。" : message;
            currentLogBox->SelectionStart = 0;
            currentLogBox->SelectionLength = 0;
        }

        void SetActionEnabledState(bool enabled)
        {
            extractTabButton->Enabled = enabled;
            packTabButton->Enabled = enabled;
            extractPanel->Enabled = enabled;
            packPanel->Enabled = enabled;
            openResultButton->Enabled = enabled && !String::IsNullOrWhiteSpace(lastOpenTarget);
            resetStateButton->Enabled = enabled;
        }

        void SetLastOpenTarget(String ^ path)
        {
            lastOpenTarget = ResolveOptionalPath(path);
            openResultButton->Enabled = !worker->IsBusy && !String::IsNullOrWhiteSpace(lastOpenTarget);
        }

        void SetIdleState(String ^ statusText, String ^ logText)
        {
            UseWaitCursor = false;
            busyStatusPrefix = String::Empty;
            SetActionEnabledState(true);
            statusLabel->Text = statusText;
            progressBar->Value = 0;
            SetCurrentLog(logText);
        }

        void SetBusyState(String ^ statusText, String ^ logText)
        {
            UseWaitCursor = true;
            busyStatusPrefix = statusText;
            SetActionEnabledState(false);
            statusLabel->Text = statusText;
            progressBar->Value = 0;
            SetCurrentLog(logText);
        }

        bool EnsureNotBusy()
        {
            if (worker->IsBusy)
            {
                statusLabel->Text = L"当前已有任务在运行，请等待完成。";
                return false;
            }
            return true;
        }

        void OpenPathInExplorer(String ^ path)
        {
            if (String::IsNullOrWhiteSpace(path))
                return;
            String ^ full = Path::GetFullPath(path);
            if (File::Exists(full))
            {
                Process::Start(L"explorer.exe", L"/select,\"" + full + L"\"");
                return;
            }
            if (!Directory::Exists(full))
                Directory::CreateDirectory(full);
            Process::Start(L"explorer.exe", full);
        }

        int CountSupportedFiles(array<String ^> ^ files)
        {
            if (files == nullptr)
                return 0;
            int n = 0;
            for each (String ^ f in files)
            {
                if (Path::GetExtension(f)->Equals(L".pac", StringComparison::OrdinalIgnoreCase))
                    ++n;
            }
            return n;
        }
        bool IsSupportedDrop(array<String ^> ^ files) { return CountSupportedFiles(files) > 0; }

        void StartWork(WorkRequest ^ req, String ^ status, String ^ log)
        {
            SetBusyState(status, log);
            worker->RunWorkerAsync(req);
        }

        void OnDragEnter(Object ^, DragEventArgs ^ e)
        {
            if (worker->IsBusy)
            {
                e->Effect = DragDropEffects::None;
                return;
            }
            if (!e->Data->GetDataPresent(DataFormats::FileDrop))
            {
                e->Effect = DragDropEffects::None;
                return;
            }
            array<String ^> ^ files = safe_cast<array<String ^> ^>(e->Data->GetData(DataFormats::FileDrop));
            e->Effect = IsSupportedDrop(files) ? DragDropEffects::Copy : DragDropEffects::None;
        }

        void OnDragDrop(Object ^, DragEventArgs ^ e)
        {
            if (!EnsureNotBusy())
                return;
            if (!e->Data->GetDataPresent(DataFormats::FileDrop))
                return;
            array<String ^> ^ files = safe_cast<array<String ^> ^>(e->Data->GetData(DataFormats::FileDrop));
            if (!IsSupportedDrop(files))
            {
                SetIdleState(L"没有检测到可处理的 .pac 文件。", L"等待拖入 .pac 文件。");
                return;
            }
            WorkRequest ^ req = gcnew WorkRequest();
            req->kind = UiWorkKind::Extract;
            req->files = files;
            int n = CountSupportedFiles(files);
            StartWork(req, L"正在解包 " + n.ToString() + L" 个 .pac ...", L"准备开始解包任务...");
        }

        void OnRefreshDefaultsClicked(Object ^, EventArgs ^)
        {
            if (!EnsureNotBusy())
                return;
            RefreshPackDefaults();
            SetIdleState(L"已刷新默认路径  ❖", L"Pack 页签的默认值已就绪。");
        }
        void OnBrowseScriptInClicked(Object ^, EventArgs ^)
        {
            OpenFileDialog ^ d = gcnew OpenFileDialog();
            d->Filter = L"SoftPal 脚本|*.SRC;SCRIPT.SRC|所有文件|*.*";
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
            {
                scriptInTextBox->Text = d->FileName;
                String ^ dir = Path::GetDirectoryName(d->FileName);
                String ^ guess = Path::Combine(dir, L"TEXT.DAT");
                if (File::Exists(guess) && String::IsNullOrWhiteSpace(textInTextBox->Text))
                    textInTextBox->Text = guess;
                if (String::IsNullOrWhiteSpace(jsonOutTextBox->Text))
                    jsonOutTextBox->Text = Path::Combine(dir, L"script_export.json");
            }
        }
        void OnBrowseTextInClicked(Object ^, EventArgs ^)
        {
            OpenFileDialog ^ d = gcnew OpenFileDialog();
            d->Filter = L"SoftPal 文本|*.DAT;TEXT.DAT|所有文件|*.*";
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
                textInTextBox->Text = d->FileName;
        }
        void OnBrowseJsonOutClicked(Object ^, EventArgs ^)
        {
            SaveFileDialog ^ d = gcnew SaveFileDialog();
            d->Filter = L"JSON|*.json|所有文件|*.*";
            d->FileName = String::IsNullOrWhiteSpace(jsonOutTextBox->Text)
                              ? L"script_export.json"
                              : Path::GetFileName(jsonOutTextBox->Text);
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
                jsonOutTextBox->Text = d->FileName;
        }
        void OnBrowseRbJsonClicked(Object ^, EventArgs ^)
        {
            OpenFileDialog ^ d = gcnew OpenFileDialog();
            d->Filter = L"JSON|*.json|所有文件|*.*";
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
                rbJsonTextBox->Text = d->FileName;
        }
        void OnBrowseRbOutDirClicked(Object ^, EventArgs ^)
        {
            FolderBrowserDialog ^ d = gcnew FolderBrowserDialog();
            d->Description = L"选择重建输出目录（生成 SCRIPT.SRC + TEXT.DAT）";
            if (Directory::Exists(rbOutDirTextBox->Text))
                d->SelectedPath = rbOutDirTextBox->Text;
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
                rbOutDirTextBox->Text = d->SelectedPath;
        }
        void OnBrowsePalInClicked(Object ^, EventArgs ^)
        {
            OpenFileDialog ^ d = gcnew OpenFileDialog();
            d->Filter = L"所有文件|*.*";
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
            {
                palInTextBox->Text = d->FileName;
                if (String::IsNullOrWhiteSpace(palOutTextBox->Text))
                    palOutTextBox->Text = d->FileName + L".dec";
            }
        }
        void OnBrowsePalOutClicked(Object ^, EventArgs ^)
        {
            SaveFileDialog ^ d = gcnew SaveFileDialog();
            d->Filter = L"所有文件|*.*";
            if (!String::IsNullOrWhiteSpace(palOutTextBox->Text))
                d->FileName = Path::GetFileName(palOutTextBox->Text);
            if (d->ShowDialog(this) == System::Windows::Forms::DialogResult::OK)
                palOutTextBox->Text = d->FileName;
        }

        void OnDisassembleClicked(Object ^, EventArgs ^)
        {
            if (!EnsureNotBusy())
                return;
            if (String::IsNullOrWhiteSpace(scriptInTextBox->Text) || !File::Exists(scriptInTextBox->Text))
            {
                SetIdleState(L"无法开始反汇编。", L"请先指定存在的 SCRIPT.SRC。");
                return;
            }
            if (String::IsNullOrWhiteSpace(textInTextBox->Text) || !File::Exists(textInTextBox->Text))
            {
                SetIdleState(L"无法开始反汇编。", L"请先指定存在的 TEXT.DAT。");
                return;
            }
            if (String::IsNullOrWhiteSpace(jsonOutTextBox->Text))
            {
                SetIdleState(L"无法开始反汇编。", L"请先指定输出 JSON 路径。");
                return;
            }
            WorkRequest ^ r = gcnew WorkRequest();
            r->kind = UiWorkKind::DisassembleScript;
            r->scriptInPath = scriptInTextBox->Text;
            r->textInPath = textInTextBox->Text;
            r->jsonPath = jsonOutTextBox->Text;
            StartWork(r, L"正在反汇编脚本...", L"扫描 SCRIPT.SRC 中的对话条目...");
        }
        void OnRebuildClicked(Object ^, EventArgs ^)
        {
            if (!EnsureNotBusy())
                return;
            if (String::IsNullOrWhiteSpace(scriptInTextBox->Text) || !File::Exists(scriptInTextBox->Text))
            {
                SetIdleState(L"无法开始重建。", L"请先在「脚本反汇编」一栏指定原 SCRIPT.SRC。");
                return;
            }
            if (String::IsNullOrWhiteSpace(textInTextBox->Text) || !File::Exists(textInTextBox->Text))
            {
                SetIdleState(L"无法开始重建。", L"请先在「脚本反汇编」一栏指定原 TEXT.DAT。");
                return;
            }
            if (String::IsNullOrWhiteSpace(rbJsonTextBox->Text) || !File::Exists(rbJsonTextBox->Text))
            {
                SetIdleState(L"无法开始重建。", L"请先指定翻译 JSON。");
                return;
            }
            if (String::IsNullOrWhiteSpace(rbOutDirTextBox->Text))
            {
                SetIdleState(L"无法开始重建。", L"请先指定输出目录。");
                return;
            }
            WorkRequest ^ r = gcnew WorkRequest();
            r->kind = UiWorkKind::RebuildScript;
            r->scriptInPath = scriptInTextBox->Text;
            r->textInPath = textInTextBox->Text;
            r->jsonPath = rbJsonTextBox->Text;
            r->scriptOutPath = Path::Combine(rbOutDirTextBox->Text, L"SCRIPT.SRC");
            r->textOutPath = Path::Combine(rbOutDirTextBox->Text, L"TEXT.DAT");
            r->encoding = dynamic_cast<String ^>(rbEncodingComboBox->SelectedItem);
            if (String::IsNullOrWhiteSpace(r->encoding))
                r->encoding = L"gbk";
            StartWork(r, L"正在重建脚本...", L"按 JSON 写回 SCRIPT.SRC 与 TEXT.DAT...");
        }
        void OnPalDecryptClicked(Object ^, EventArgs ^)
        {
            if (!EnsureNotBusy())
                return;
            if (String::IsNullOrWhiteSpace(palInTextBox->Text) || !File::Exists(palInTextBox->Text))
            {
                SetIdleState(L"无法开始解密。", L"请先指定存在的输入文件。");
                return;
            }
            WorkRequest ^ r = gcnew WorkRequest();
            r->kind = UiWorkKind::PalDecrypt;
            r->palInPath = palInTextBox->Text;
            r->palOutPath = String::IsNullOrWhiteSpace(palOutTextBox->Text)
                                ? (palInTextBox->Text + L".dec")
                                : palOutTextBox->Text;
            StartWork(r, L"正在解密...", L"运行 ROL+XOR 解密...");
        }

        void OnOpenResultClicked(Object ^, EventArgs ^)
        {
            if (!String::IsNullOrWhiteSpace(lastOpenTarget))
            {
                OpenPathInExplorer(lastOpenTarget);
                return;
            }
            OpenPathInExplorer(GetDefaultUnpackFullPath());
        }
        void OnResetStateClicked(Object ^, EventArgs ^)
        {
            if (!EnsureNotBusy())
                return;
            SetLastOpenTarget(GetDefaultUnpackFullPath());
            SetIdleState(L"状态已重置  ❖", L"可以拖入下一个 .pac，或在 Pack 页签继续操作。");
        }

        void OnWorkerDoWork(Object ^, DoWorkEventArgs ^ e)
        {
            WorkRequest ^ req = safe_cast<WorkRequest ^>(e->Argument);
            WorkResult ^ res = gcnew WorkResult();
            try
            {
                switch (req->kind)
                {
                case UiWorkKind::Extract:
                    ExecuteExtract(req, res);
                    break;
                case UiWorkKind::DisassembleScript:
                    ExecuteDisassemble(req, res);
                    break;
                case UiWorkKind::RebuildScript:
                    ExecuteRebuild(req, res);
                    break;
                case UiWorkKind::PalDecrypt:
                    ExecutePalDecrypt(req, res);
                    break;
                default:
                    throw gcnew InvalidOperationException(L"未知任务类型。");
                }
            }
            catch (Exception ^ ex)
            {
                res->failureCount = res->handledCount > 0 ? res->handledCount : 1;
                res->summary = L"任务失败。";
                res->detail = ex->Message;
                e->Result = res;
                return;
            }
            e->Result = res;
        }

        void ExecuteExtract(WorkRequest ^ req, WorkResult ^ res)
        {
            String ^ lastOut = GetDefaultUnpackFullPath();
            uint32_t total_pgd = 0, ok_pgd = 0, fail_pgd = 0;
            for each (String ^ filePath in req->files)
            {
                if (!Path::GetExtension(filePath)->Equals(L".pac", StringComparison::OrdinalIgnoreCase))
                    continue;
                ++res->handledCount;
                String ^ outDir = GetDefaultUnpackDirForPac(filePath);
                String ^ pacName = Path::GetFileName(filePath);
                try
                {
                    pin_ptr<const wchar_t> wpac = PtrToStringChars(filePath);
                    sp_pac_archive *arc = sp_pac_open(wpac);
                    if (!arc)
                        throw gcnew InvalidOperationException(gcnew String(sp_last_error()));
                    try
                    {
                        pin_ptr<const wchar_t> wout = PtrToStringChars(outDir);
                        phaseLabel = L"解包 " + pacName;
                        if (!sp_pac_extract_all(arc, wout, progressCallback, nullptr))
                            throw gcnew InvalidOperationException(gcnew String(sp_last_error()));
                    }
                    finally
                    {
                        sp_pac_close(arc);
                    }
                    // 解包后自动跑 PGD→PNG，core 内部已保证 GE 底图先于 PGD3 叠图处理；
                    // delete_pgd=1 让目录里只留可用的 PNG，不留原始 .PGD
                    {
                        pin_ptr<const wchar_t> wout = PtrToStringChars(outDir);
                        uint32_t t = 0, o = 0, f = 0;
                        phaseLabel = L"PGD→PNG " + pacName;
                        sp_dir_convert_pgd(wout, /*delete_pgd=*/1, &t, &o, &f,
                                           progressCallback, nullptr);
                        total_pgd += t;
                        ok_pgd += o;
                        fail_pgd += f;
                    }
                    ++res->successCount;
                    lastOut = outDir;
                    res->detail = L"完成: " + pacName + L" → " + outDir;
                }
                catch (Exception ^ ex)
                {
                    ++res->failureCount;
                    res->detail = L"失败: " + pacName + L" → " + ex->Message;
                }
            }
            phaseLabel = String::Empty;
            res->primaryPath = lastOut;
            String ^ pgdLine = (total_pgd > 0)
                                   ? (L"  |  PGD→PNG: " + ok_pgd.ToString() + L"/" + total_pgd.ToString() + (fail_pgd > 0 ? (L" (失败 " + fail_pgd.ToString() + L")") : String::Empty))
                                   : String::Empty;
            res->summary = (res->failureCount == 0
                                ? (L"解包完成，共处理 " + res->handledCount.ToString() + L" 个文件。")
                                : (L"解包结束，成功 " + res->successCount.ToString() + L" 个，失败 " + res->failureCount.ToString() + L" 个。")) +
                           pgdLine;
        }

        void ExecuteDisassemble(WorkRequest ^ req, WorkResult ^ res)
        {
            pin_ptr<const wchar_t> w1 = PtrToStringChars(req->scriptInPath);
            pin_ptr<const wchar_t> w2 = PtrToStringChars(req->textInPath);
            pin_ptr<const wchar_t> w3 = PtrToStringChars(req->jsonPath);
            if (!sp_script_to_json(w1, w2, w3))
                throw gcnew InvalidOperationException(gcnew String(sp_last_error()));
            res->handledCount = 1;
            res->successCount = 1;
            res->primaryPath = req->jsonPath;
            res->summary = L"反汇编完成  ❖";
            res->detail = L"输出 JSON: " + req->jsonPath;
        }

        void ExecuteRebuild(WorkRequest ^ req, WorkResult ^ res)
        {
            String ^ outDir = Path::GetDirectoryName(req->scriptOutPath);
            if (!String::IsNullOrWhiteSpace(outDir) && !Directory::Exists(outDir))
                Directory::CreateDirectory(outDir);

            pin_ptr<const wchar_t> w1 = PtrToStringChars(req->scriptInPath);
            pin_ptr<const wchar_t> w2 = PtrToStringChars(req->textInPath);
            pin_ptr<const wchar_t> w3 = PtrToStringChars(req->jsonPath);
            pin_ptr<const wchar_t> w4 = PtrToStringChars(req->scriptOutPath);
            pin_ptr<const wchar_t> w5 = PtrToStringChars(req->textOutPath);

            IntPtr encPtr = Marshal::StringToHGlobalAnsi(req->encoding);
            try
            {
                const char *enc = static_cast<const char *>(encPtr.ToPointer());
                if (!sp_script_rebuild(w1, w2, w3, w4, w5, enc))
                    throw gcnew InvalidOperationException(gcnew String(sp_last_error()));
            }
            finally
            {
                Marshal::FreeHGlobal(encPtr);
            }
            res->handledCount = 1;
            res->successCount = 1;
            res->primaryPath = req->scriptOutPath;
            res->secondaryPath = req->textOutPath;
            res->summary = L"重建完成  ❖";
            res->detail = L"输出 SCRIPT.SRC: " + req->scriptOutPath + L" | 输出 TEXT.DAT: " + req->textOutPath;
        }

        void ExecutePalDecrypt(WorkRequest ^ req, WorkResult ^ res)
        {
            pin_ptr<const wchar_t> win = PtrToStringChars(req->palInPath);
            pin_ptr<const wchar_t> wout = PtrToStringChars(req->palOutPath);
            if (!sp_pal_decrypt_file(win, wout))
                throw gcnew InvalidOperationException(gcnew String(sp_last_error()));
            res->handledCount = 1;
            res->successCount = 1;
            res->primaryPath = req->palOutPath;
            res->summary = L"PAL 解密完成  ❖";
            res->detail = L"输出文件: " + req->palOutPath;
        }

        void OnWorkerCompleted(Object ^, RunWorkerCompletedEventArgs ^ e)
        {
            if (e->Error != nullptr)
            {
                SetLastOpenTarget(GetDefaultUnpackFullPath());
                SetIdleState(L"任务失败。", e->Error->Message);
                return;
            }
            WorkResult ^ res = safe_cast<WorkResult ^>(e->Result);
            if (res == nullptr)
            {
                SetLastOpenTarget(GetDefaultUnpackFullPath());
                SetIdleState(L"没有产生任何结果。", L"请重新执行操作。");
                return;
            }
            progressBar->Value = res->failureCount > 0 ? 0 : 100;
            statusLabel->Text = res->summary;
            UseWaitCursor = false;
            busyStatusPrefix = String::Empty;
            SetActionEnabledState(true);
            SetCurrentLog(res->detail);
            if (!String::IsNullOrWhiteSpace(res->primaryPath))
            {
                SetLastOpenTarget(res->primaryPath);
            }
            else
            {
                SetLastOpenTarget(GetDefaultUnpackFullPath());
            }
        }
    };

} // namespace softpal

\NeedsTeXFormat{LaTeX2e}
\ProvidesClass{styles/jouthesis}[2026/05/26 Minimal English thesis class for FTCL]
\LoadClass[12pt,a4paper,openany,oneside]{book}

\RequirePackage[a4paper,left=3.0cm,right=2.6cm,top=2.8cm,bottom=2.8cm,headsep=0.55cm]{geometry}
\RequirePackage{amsmath,amssymb,amsthm}
\renewcommand{\proofname}{证明}
\RequirePackage{booktabs,longtable,tabularx,array,multirow}
\RequirePackage{graphicx,float}
\RequirePackage[ruled,vlined,linesnumbered]{algorithm2e}
\RequirePackage{xcolor}
\RequirePackage{listings}
\RequirePackage{caption}
\RequirePackage{enumitem}
\RequirePackage{fancyhdr}
\RequirePackage{lastpage}
\RequirePackage{titlesec}
\RequirePackage{indentfirst}
\RequirePackage{setspace}
\RequirePackage{fontspec}
\RequirePackage{xeCJK}
\RequirePackage{hyperref}
\RequirePackage[backend=biber,style=numeric,sorting=none]{biblatex}
\addbibresource{contents/references.bib}

% Use thesis-style superscript numeric citations instead of inline [1,2].
\newcommand{\JOUCitationSuperscript}[1]{\textsuperscript{\scriptsize #1}}
\DeclareDelimFormat{multicitedelim}{\addcomma}
\DeclareCiteCommand{\cite}[\JOUCitationSuperscript]
  {\unskip\bibopenbracket}
  {\usebibmacro{citeindex}\usebibmacro{cite}}
  {\multicitedelim}
  {\bibclosebracket}

\graphicspath{{contents/figures/}{docs/figures/}{docs/figures/architecture/}{docs/figures/geometry/}{docs/figures/benchmarks/}}

\hypersetup{
  colorlinks=true,
  linkcolor=black,
  citecolor=black,
  urlcolor=blue,
  bookmarksopen=true
}

\setstretch{1.45}
\setlength{\parindent}{2em}
\setlength{\parskip}{0pt}
\setlength{\headheight}{18pt}
\captionsetup{font=small,labelfont=bf}
\setlist{nosep,leftmargin=2.0em}
\setmainfont{Latin Modern Roman}
\setsansfont{Latin Modern Sans}
\setmonofont{Latin Modern Mono}
\IfFontExistsTF{AR PL UMing CN}{
  \setCJKmainfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{AR PL UMing CN}
  \setCJKsansfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{AR PL UMing CN}
}{
  \setCJKmainfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{FandolSong-Regular}
  \setCJKsansfont[AutoFakeBold=2.4,AutoFakeSlant=0.18]{FandolHei-Regular}
}
\IfFontExistsTF{TeX Gyre Termes}{
  \newfontfamily\JOUCoverEnglishFont{TeX Gyre Termes}
}{
  \newfontfamily\JOUCoverEnglishFont{Latin Modern Roman}
}
\IfFontExistsTF{Times New Roman}{
  \newfontfamily\JOUAbstractEnglishFont{Times New Roman}
}{
  \IfFontExistsTF{TeX Gyre Termes}{
    \newfontfamily\JOUAbstractEnglishFont{TeX Gyre Termes}
  }{
    \newfontfamily\JOUAbstractEnglishFont{Latin Modern Roman}
  }
}

\SetAlgoCaptionSeparator{:}
\SetAlFnt{\small}
\SetAlgoNlRelativeSize{-1}
\SetAlgoInsideSkip{smallskip}
\SetKwInput{KwInput}{Input}
\SetKwInput{KwData}{Data}
\SetKwInput{KwOutput}{Output}
\SetKw{KwReturn}{return}
\SetKwProg{Fn}{Function}{:}{end}
\SetKwProg{Proc}{Procedure}{:}{end}

\renewcommand{\contentsname}{目\quad 录}
\renewcommand{\figurename}{图}
\renewcommand{\tablename}{表}

\titleformat{\chapter}[hang]
  {\centering\bfseries\fontsize{16pt}{24pt}\selectfont}
  {第\thechapter 章}
  {1em}
  {}
\titlespacing*{\chapter}{0pt}{0.2cm}{1.0cm}
\titleformat{\section}
  {\bfseries\fontsize{14pt}{22pt}\selectfont}
  {\thesection}
  {0.8em}
  {}
\titleformat{\subsection}
  {\bfseries\fontsize{12.5pt}{20pt}\selectfont}
  {\thesubsection}
  {0.8em}
  {}

\newcommand{\JOUMainHeader}{%
  \small
  江苏海洋大学二〇二六届本科毕业设计（论文）%
  \hfill 理工农医类%
  \hfill 第\quad\thepage\quad 页\quad 共\quad\pageref{LastPage}\quad 页%
}

\fancypagestyle{joufront}{%
  \fancyhf{}%
  \renewcommand{\headrulewidth}{0pt}%
  \renewcommand{\footrulewidth}{0pt}%
}
\fancypagestyle{joumain}{%
  \fancyhf{}%
  \fancyhead[C]{\JOUMainHeader}%
  \renewcommand{\headrulewidth}{0.4pt}%
  \renewcommand{\footrulewidth}{0pt}%
}
\pagestyle{joufront}
\fancypagestyle{plain}{%
  \fancyhf{}%
  \renewcommand{\headrulewidth}{0pt}%
  \renewcommand{\footrulewidth}{0pt}%
}
\let\JOUOriginalFrontmatter\frontmatter
\renewcommand{\frontmatter}{%
  \JOUOriginalFrontmatter
  \pagestyle{joufront}%
  \fancypagestyle{plain}{%
    \fancyhf{}%
    \renewcommand{\headrulewidth}{0pt}%
    \renewcommand{\footrulewidth}{0pt}%
  }%
}
\let\JOUOriginalMainmatter\mainmatter
\renewcommand{\mainmatter}{%
  \JOUOriginalMainmatter
  \pagestyle{joumain}%
  \fancypagestyle{plain}{%
    \fancyhf{}%
    \fancyhead[C]{\JOUMainHeader}%
    \renewcommand{\headrulewidth}{0.4pt}%
    \renewcommand{\footrulewidth}{0pt}%
  }%
}

\lstdefinestyle{ftclstyle}{
  basicstyle=\ttfamily\small,
  breaklines=true,
  columns=fullflexible,
  frame=single,
  rulecolor=\color{black!25},
  backgroundcolor=\color{black!3},
  keywordstyle=\color{blue!70!black}\bfseries,
  commentstyle=\color{green!40!black},
  stringstyle=\color{orange!60!black}
}
\lstset{style=ftclstyle}

\newcommand{\JOUThesisTitleCN}{A Tcl-style CPU/CUDA Hybrid Computational Geometry Scripting System}
\newcommand{\JOUThesisTitleEN}{A Tcl-style CPU/CUDA Hybrid Computational Geometry Scripting System}
\newcommand{\JOUAuthor}{Fang Hongming}
\newcommand{\JOUStudentID}{To be filled}
\newcommand{\JOUCollege}{To be filled}
\newcommand{\JOUMajor}{Computer Science and Technology}
\newcommand{\JOUAdvisor}{To be filled}
\newcommand{\JOUDate}{May 2026}

\newcommand{\JOUTitleCN}[1]{\gdef\JOUThesisTitleCN{#1}}
\newcommand{\JOUTitleEN}[1]{\gdef\JOUThesisTitleEN{#1}}
\newcommand{\JOUSetAuthor}[1]{\gdef\JOUAuthor{#1}}
\newcommand{\JOUSetStudentID}[1]{\gdef\JOUStudentID{#1}}
\newcommand{\JOUSetCollege}[1]{\gdef\JOUCollege{#1}}
\newcommand{\JOUSetMajor}[1]{\gdef\JOUMajor{#1}}
\newcommand{\JOUSetAdvisor}[1]{\gdef\JOUAdvisor{#1}}
\newcommand{\JOUSetDate}[1]{\gdef\JOUDate{#1}}
\newcommand{\JOUSharedApplyToThesis}{}

\newlength{\JOUCoverLabelWidth}
\setlength{\JOUCoverLabelWidth}{3.8cm}
\newlength{\JOUCoverValueWidth}
\setlength{\JOUCoverValueWidth}{8.4cm}
\newlength{\JOUCoverShortValueWidth}
\setlength{\JOUCoverShortValueWidth}{2.9cm}
% -----------------------------------------------------------------------------
% Cover layout parameters
% -----------------------------------------------------------------------------

\newcommand{\JOUCoverHeaderImage}{jou-cover-header-crop.png}

% 整个封面内容距离页面顶部的距离。
% 调整它会带动校徽、主标题、题目等整体上下移动。
\newcommand{\JOUCoverHeaderTopVskip}{1.25cm}

% 仅调整校徽图片的视觉位置。
% 正值表示上移，负值表示下移；不会影响下方标题的位置。
\newcommand{\JOUCoverHeaderVisualRaise}{0pt}

% 校徽图片与“本科毕业设计（论文）”之间的版面间距。
\newcommand{\JOUCoverMainTitleVskip}{0.55cm}

% “本科毕业设计（论文）”这一行自身的微调。
% 正值表示上移，负值表示下移。
\newcommand{\JOUCoverMainTitleRaise}{0.15ex}

% “本科毕业设计（论文）”与“毕业设计（论文）中文题目”之间的距离。
\newcommand{\JOUCoverSubtitleVskip}{3.35cm}

\newcommand{\JOUCoverIncludeHeaderImage}{%
  \includegraphics[
    width=12.2cm
  ]{\JOUCoverHeaderImage}%
}

% 放置校徽图片。
\newcommand{\JOUCoverPlaceHeaderImage}{%
  \JOUCoverIncludeHeaderImage
}

\newcommand{\JOUCoverCJKBold}[1]{{\sffamily\bfseries #1}}
\newcommand{\JOUCoverUnderline}[2]{%
  \makebox[#1][c]{#2}\leaders\hrule height 0.42pt\hfill\kern0pt%
}
\newcommand{\JOUCoverLine}[2]{%
  \noindent
  \makebox[\JOUCoverLabelWidth][s]{\JOUCoverCJKBold{#1}}\quad
  \makebox[\JOUCoverValueWidth][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[\JOUCoverValueWidth][c]{#2}}%
  }\par
}
\newcommand{\JOUCoverStudentLine}{%
  \noindent
  \makebox[\JOUCoverLabelWidth][s]{\JOUCoverCJKBold{学生姓名：}}\quad
  \makebox[3.4cm][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[3.4cm][c]{\JOUAuthor}}%
  }%
  \hspace{0.45cm}%
  \makebox[1.35cm][s]{\JOUCoverCJKBold{学号：}}%
  \makebox[3.0cm][l]{%
    \leaders\hrule height 0.42pt\hfill
    \llap{\makebox[3.0cm][c]{\JOUStudentID}}%
  }\par
}

\newcommand{\JOUMySign}{%
  \raisebox{-0.4ex}{\includegraphics[height=1.15cm]{mysign.png}}%
}
\newcommand{\JOUTutorSign}{%
  \raisebox{-0.4ex}{\includegraphics[height=1.15cm]{tutorsign.jpg}}%
}

\newcommand{\JOUDeclarationPage}{%
  \clearpage
  \thispagestyle{empty}
  \newgeometry{left=2.8cm,right=2.8cm,top=2.1cm,bottom=2.2cm}
  \begin{center}
    {\sffamily\bfseries\fontsize{18pt}{28pt}\selectfont 江苏海洋大学\par}
    \vspace{0.15cm}
    {\sffamily\bfseries\fontsize{18pt}{28pt}\selectfont 毕业设计（论文）原创性声明\par}
  \end{center}
  \vspace{0.75cm}
  {\fontsize{13pt}{23pt}\selectfont
  本人声明所提交的毕业设计（论文）是在导师的指导下，独立完成的研究成果。
  除文中已经注明引用的内容外，本论文不包含任何其他个人或者集体已经发表或者撰写过的成果。
  本文的研究成果是经过本人的辛勤劳动取得的，对本研究做出重要贡献的个人和集体，均已在文中明确表示出来。
  本人完全意识到本声明所涉及的法律后果由本人承担。\par}
  \vspace{0.75cm}
  {\fontsize{13pt}{23pt}\selectfont
  作者签名：\JOUMySign\hfill 日期：\hspace{1.1cm}年\hspace{1.0cm}月\hspace{1.0cm}日\par}
  \vspace{1.45cm}
  \begin{center}
    {\sffamily\bfseries\fontsize{18pt}{28pt}\selectfont 江苏海洋大学\par}
    \vspace{0.15cm}
    {\sffamily\bfseries\fontsize{18pt}{28pt}\selectfont 毕业设计（论文）版权使用授权书\par}
  \end{center}
  \vspace{0.65cm}
  {\fontsize{13pt}{23pt}\selectfont
  本毕业设计（论文）作者同意学校保存并送交国家有关部门或机构有关毕业设计（论文）的复印件、电子版，允许论文被查阅和借阅。
  本人同意江苏海洋大学可以将本毕业设计（论文）的全部或部分内容编入有关数据库进行检索，可以采用影印、缩印或者扫描等复制手段保存和汇编本毕业设计（论文）。\par}
  \vspace{1.1cm}
  {\fontsize{13pt}{23pt}\selectfont
  作者签名：\JOUMySign\hfill 指导教师签名：\JOUTutorSign\par
  \vspace{0.45cm}
  日期：\hspace{1.0cm}年\hspace{1.0cm}月\hspace{1.0cm}日\hfill
  日期：\hspace{1.0cm}年\hspace{1.0cm}月\hspace{1.0cm}日\par}
  \restoregeometry
  \clearpage
}

\newcommand{\makecover}{%
  \begin{titlepage}
    \thispagestyle{empty}
    \newgeometry{left=2.6cm,right=2.6cm,top=1.55cm,bottom=1.8cm}
    \centering
    \vspace*{\JOUCoverHeaderTopVskip}

    {\JOUCoverPlaceHeaderImage}\par

    \vspace{\JOUCoverMainTitleVskip}
    {\raisebox{\JOUCoverMainTitleRaise}{%
      \sffamily\bfseries\fontsize{24pt}{32pt}\selectfont
      本\quad 科\quad 毕\quad 业\quad 设\quad 计\quad （论\quad 文）%
    }\par}
    \vspace{\JOUCoverSubtitleVskip}
    \begin{minipage}{0.88\textwidth}
      \centering
      {\sffamily\bfseries\fontsize{16pt}{26pt}\selectfont \JOUThesisTitleCN\par}
    \end{minipage}\par
    \vspace{0.56cm}
    \begin{minipage}{0.88\textwidth}
      \centering
      {\JOUCoverEnglishFont\bfseries\fontsize{16pt}{22pt}\selectfont \JOUThesisTitleEN\par}
    \end{minipage}\par
    \vfill
    \begin{minipage}{12.7cm}
      \fontsize{14pt}{25pt}\selectfont
      \JOUCoverLine{学院：}{\JOUCollege}
      \vspace{0.18cm}
      \JOUCoverLine{专业班级：}{\JOUMajor}
      \vspace{0.18cm}
      \JOUCoverStudentLine
      \vspace{0.18cm}
      \JOUCoverLine{指导教师：}{\JOUAdvisor}
    \end{minipage}\par
    \vspace{1.02cm}
    {\sffamily\bfseries\fontsize{16pt}{24pt}\selectfont 2026 年\qquad 5 月\par}
    \restoregeometry
  \end{titlepage}
  \JOUDeclarationPage
}

\newsavebox{\JOUAbstractSaveBox}
\newlength{\JOUAbstractInnerWidth}
\setlength{\JOUAbstractInnerWidth}{\dimexpr\textwidth-0.9cm-2\fboxrule\relax}

\newenvironment{cnabstract}{%
  \clearpage
  \newgeometry{left=2.35cm,right=2.35cm,top=0.8cm,bottom=1.0cm}
  \thispagestyle{empty}
  \begin{center}
    {\sffamily\bfseries\fontsize{18pt}{30pt}\selectfont 毕业设计（论文）中文摘要\par}
  \end{center}
  \vspace{0.25cm}
  \setlength{\fboxrule}{0.8pt}
  \setlength{\fboxsep}{0.35cm}
  \setlength{\JOUAbstractInnerWidth}{\dimexpr\textwidth-0.7cm-2\fboxrule\relax}
  \begin{lrbox}{\JOUAbstractSaveBox}%
    \begin{minipage}[t][25.1cm][t]{\JOUAbstractInnerWidth}
      \setlength{\parindent}{0pt}%
      \setlength{\parskip}{0pt}%
      \fontsize{10.5pt}{16pt}\selectfont%
      \vspace*{0.95cm}%
      \begin{center}
        {\fontsize{14pt}{21pt}\selectfont\JOUThesisTitleCN\par}
      \end{center}
      \vspace{0.9cm}%
}{%
    \end{minipage}%
  \end{lrbox}%
  \noindent\fbox{\usebox{\JOUAbstractSaveBox}}%
  \restoregeometry
}
\newcommand{\cnabstracttext}[1]{%
  \noindent{\sffamily\bfseries\fontsize{12pt}{18pt}\selectfont 摘\quad 要：}%
  {\fontsize{10.5pt}{16pt}\selectfont #1\par}%
}
\newcommand{\cnkeywords}[1]{%
  \par\vspace{1em}\noindent{\sffamily\bfseries\fontsize{12pt}{18pt}\selectfont 关键字：}%
  {\fontsize{10.5pt}{16pt}\selectfont #1}%
}

\newenvironment{enabstract}{%
  \clearpage
  \newgeometry{left=2.35cm,right=2.35cm,top=0.8cm,bottom=1.0cm}
  \thispagestyle{empty}
  \begin{center}
    {\sffamily\bfseries\fontsize{18pt}{30pt}\selectfont 毕业设计（论文）外文摘要\par}
  \end{center}
  \vspace{0.25cm}
  \setlength{\fboxrule}{0.8pt}
  \setlength{\fboxsep}{0.35cm}
  \setlength{\JOUAbstractInnerWidth}{\dimexpr\textwidth-0.7cm-2\fboxrule\relax}
  \begin{lrbox}{\JOUAbstractSaveBox}%
    \begin{minipage}[t][25.1cm][t]{\JOUAbstractInnerWidth}
      \JOUAbstractEnglishFont%
      \setlength{\parindent}{0pt}%
      \setlength{\parskip}{0pt}%
      \fontsize{10.5pt}{16pt}\selectfont%
      \vspace*{1.05cm}%
      \begin{center}
        {\bfseries\fontsize{14pt}{21pt}\selectfont\JOUThesisTitleEN\par}
      \end{center}
      \vspace{1.0cm}%
}{%
    \end{minipage}%
  \end{lrbox}%
  \noindent\fbox{\usebox{\JOUAbstractSaveBox}}%
  \restoregeometry
}
\newcommand{\enabstracttext}[1]{%
  \noindent{\bfseries Abstract: }#1\par%
}
\newcommand{\enkeywords}[1]{%
  \par\vspace{1em}\noindent{\bfseries Keywords: }#1%
}

\newcommand{\JOUListOfTablesAndFigures}{%
  \clearpage
}

\newcommand{\JOUBackmatterChapter}[1]{\chapter{#1}}
\newcommand{\JOUAcknowledgementChapter}{%
  \chapter*{致\quad 谢}%
  \addcontentsline{toc}{chapter}{致谢}%
  \@afterindenttrue
}
\newcommand{\JOUPrintBibliography}{%
  \cleardoublepage
  \printbibliography[heading=bibintoc,title={参考文献}]
}

import "./Main.scss";
import TitleBar from "./TitleBar/TitleBar";
import ContentBox from "./ContentBox/ContentBox";
import StatusBar from "./StatusBar/StatusBar";
import WindowBorder from "./WindowBorder/WindowBorder";
import Menu from "./Menu/Menu";
import Msg from "./Msg";

const body = document.querySelector<HTMLElement>("body")!;

TitleBar.appendTo(body);
ContentBox.appendTo(body);
StatusBar.appendTo(body);
WindowBorder.appendTo(body);
// 右键菜单是全局的：挂到 body 上，由 KnowList / KnowNet 在需要时 open
Menu.appendTo(body);

Msg.invoke("win.show");

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "jsoncpp/json.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include "fan_calculator.h"

using namespace std;

Json::Value inputValue, outputValue;

struct PlayerData {
    struct Pack {
        string type;
        //"PENG" "GANG" "CHI"
        string tile;
        int offer;
        //PENG GANG 提供者
        //CHI 第几张牌是上家的
        Pack(string type, string tile, int offer) :
            type(type), tile(tile), offer(offer)
        {
        }
    };
    vector<Pack> pack;
    vector<string> tile;
    vector<string> flower;
};
PlayerData playerData[4];
string lastTile;
string lastOp;
//"DRAW"
//"FLOWER"
//"PLAY"
//"PENG"
//"GANG"
//"CHI"
string tileCHI;
bool lastBUGANG = false;
//上一回合是否为补杠
bool currBUGANG = false;
//当前回合是否为补杠
//与抢杠和 杠上开花有关

int roundStage = -2;
//-2:通知位置
//-1:发牌
//0-3:玩家摸牌
//4-7:玩家打出牌后，通知所有玩家
//8-12:玩家杠牌，通知所有玩家
int quan;
vector<string> tileWall;

unordered_map<string, mahjong::tile_t> str2tile;

unordered_map<string, int> shownTile;
//记录已经明示的牌，用于和绝张

void playerError(int player, const string code)
{
    outputValue["display"]["action"] = code;
    outputValue["display"]["player"] = player;
    outputValue["command"] = "finish";
    for(int i = 0; i < 4; i++) {
        if(i == player) {
            outputValue["display"]["score"][i] = -30;
            outputValue["content"][to_string(i)] = -30;
        } else {
            outputValue["display"]["score"][i] = 10;
            outputValue["content"][to_string(i)] = 10;
        }
    }
    cout << outputValue;
    exit(0);
}

//若finish=true，表示bot选择和牌，进入finish阶段
int checkHu(int player, bool finish) {
    mahjong::calculate_param_t calculate_param;
    mahjong::fan_table_t fan_table;
    memset(&calculate_param, 0, sizeof(mahjong::calculate_param_t));
    memset(&fan_table, 0, sizeof(mahjong::fan_table_t));
    calculate_param.hand_tiles.tile_count = playerData[player].tile.size();
    for(unsigned int i = 0; i < playerData[player].tile.size(); i++) {
        calculate_param.hand_tiles.standing_tiles[i] = str2tile[playerData[player].tile[i]];
    }
    calculate_param.hand_tiles.pack_count = playerData[player].pack.size();
    for(unsigned int i = 0; i < playerData[player].pack.size(); i++) {
        PlayerData::Pack &sPack = playerData[player].pack[i];
        mahjong::pack_t &dPack = calculate_param.hand_tiles.fixed_packs[i];
        if(sPack.type == "PENG") {
            dPack = mahjong::make_pack((sPack.offer - player + 4) % 4, PACK_TYPE_PUNG, str2tile[sPack.tile]);
        } else if(sPack.type == "GANG") {
            dPack = mahjong::make_pack((sPack.offer - player + 4) % 4, PACK_TYPE_KONG, str2tile[sPack.tile]);
        } else {
            dPack = mahjong::make_pack(sPack.offer + 1, PACK_TYPE_CHOW, str2tile[sPack.tile]);
        }
    }
    calculate_param.win_tile = str2tile[lastTile];
    if(player == 0) {
        //carr << playerData[player].flower.size();
    }
    calculate_param.flower_count = playerData[player].flower.size();
    if(roundStage == player) {
		// 自摸
        calculate_param.win_flag |= WIN_FLAG_SELF_DRAWN;
    }
    if(tileWall.size() == 0) {
		// 最后一张
        calculate_param.win_flag |= WIN_FLAG_WALL_LAST;
    }
    if(shownTile[lastTile] == 3) {
		// 绝张
        calculate_param.win_flag |= WIN_FLAG_4TH_TILE;
    }
    if(lastBUGANG) {
		// 抢杠胡
        calculate_param.win_flag |= WIN_FLAG_ABOUT_KONG;
    }
    calculate_param.prevalent_wind = (mahjong::wind_t)quan;
    calculate_param.seat_wind = (mahjong::wind_t)player;
    int re = mahjong::calculate_fan(&calculate_param, &fan_table);
    //cerr << re << endl;
    if(finish) {
        outputValue["display"]["action"] = "HU";
        outputValue["display"]["player"] = player;
        outputValue["display"]["fanCnt"] = re;
        for(int i = 0; i < mahjong::FAN_TABLE_SIZE; i++) {
            if(fan_table[i] > 0) {
                Json::Value cFan;
                cFan["name"] = mahjong::fan_name[i];
                cFan["cnt"] = fan_table[i];
                cFan["value"] = mahjong::fan_value_table[i];
                outputValue["display"]["fan"].append(cFan);
            }
        }
        if(re < 8) {
			// 错胡
            playerError(player, "WH");
        }
        for(int i = 0; i < 4; i++) {
            if(roundStage < 4) {
				// 处于摸牌阶段，所以分数按自摸计算
                if(i == player) {
                    outputValue["display"]["score"][i] = 3 * (8 + re);
                    outputValue["content"][to_string(i)] = 3 * (8 + re);
                } else {
                    outputValue["display"]["score"][i] = -(8 + re);
                    outputValue["content"][to_string(i)] = -(8 + re);
                }
            } else {
				// 点炮
                if(i == player) {
                    outputValue["display"]["score"][i] = (3 * 8 + re);
                    outputValue["content"][to_string(i)] = (3 * 8 + re);
                } else if(roundStage == i + 4) {
					// 点炮者
                    outputValue["display"]["score"][i] = -(8 + re);
                    outputValue["content"][to_string(i)] = -(8 + re);
                } else if(roundStage == i + 8 && lastBUGANG) {
					// 补杠被抢杠胡者
                    outputValue["display"]["score"][i] = -(8 + re);
                    outputValue["content"][to_string(i)] = -(8 + re);
                } else {
					// 非点炮者
                    outputValue["display"]["score"][i] = -8;
                    outputValue["content"][to_string(i)] = -8;
                }
            }
        }
        outputValue["command"] = "finish";
        cout << outputValue;
        exit(0);
    }
    return re;
}

//检查玩家输出是否为PASS
void checkInputPASS(const Json::Value &playerOutput, int player)
{
	// playOutput为裁判得到的player的输出
    if(playerOutput["verdict"] != "OK") {
        playerError(player, playerOutput["verdict"].asString());
    }
    if(playerOutput["response"] != "PASS") {
        playerError(player, "WA");
    }
}

//检查玩家摸牌后的输出

void checkInputDRAW(const Json::Value &playerOutput, int player)
{
    if(playerOutput["verdict"] != "OK") {
        playerError(player, playerOutput["verdict"].asString());
    }
    vector<string> outputList;
    string outputString = playerOutput["response"].asString();
    boost::split(outputList, outputString, boost::is_any_of(" "));
    if(outputList.size() == 1) {
        if(outputList[0] == "HU") {
			// 自摸胡
            checkHu(player, true);
        }
    } else if(outputList.size() == 2) {
		
        playerData[player].tile.push_back(lastTile);
		// 上回合摸的牌
        lastTile = outputList[1];
		// 打出的牌
        if(outputList[0] == "PLAY") {
            vector<string>::iterator curr = find(
                playerData[player].tile.begin(),
                playerData[player].tile.end(),
                lastTile);
            if(curr != playerData[player].tile.end()) {
                playerData[player].tile.erase(curr);
                lastOp = "PLAY";
                roundStage += 4;
                return;
            }
        } else if(outputList[0] == "GANG") {
			// 暗杠
            for(int i = 0; i < 4; i++) {
                vector<string>::iterator curr = find(
                    playerData[player].tile.begin(),
                    playerData[player].tile.end(),
                    lastTile);
                if(curr == playerData[player].tile.end()) {
                    playerError(player, "WA");
                }
                playerData[player].tile.erase(curr);
            }
            playerData[player].pack.push_back(PlayerData::Pack("GANG", lastTile, player));
            roundStage = player + 8;
            return;
        } else if(outputList[0] == "BUGANG") {
            currBUGANG = true;
            for(unsigned int i = 0; i < playerData[player].pack.size(); i++) {
				// 把之前碰的pack变成杠的pack
                if(playerData[player].pack[i].type == "PENG" &&
                        playerData[player].pack[i].tile == lastTile) {
                    playerData[player].pack[i].type = "GANG";
                    roundStage = player + 8;
                    return;
                }
            }
        }
    }
    playerError(player, "WA");
    return;
}

//检查其他玩家打出牌后，玩家的输出
//1检查和牌操作
void checkInputPLAY1(const Json::Value &playerOutput, int player)
{
    if(playerOutput["verdict"] != "OK") {
        playerError(player, playerOutput["verdict"].asString());
    }
    string outputString = playerOutput["response"].asString();
    if(outputString == "HU") {
        checkHu(player, true);
    }
}
//2检查碰牌、杠牌
bool checkInputPLAY2(const Json::Value &playerOutput, int player)
{
    vector<string> outputList;
    string outputString = playerOutput["response"].asString();
    boost::split(outputList, outputString, boost::is_any_of(" "));
    if(outputList.size() == 1) {
        if(outputList[0] == "PASS") {
            return false;
        }else if(outputList[0] == "GANG") {
			// 从立牌中移除三张
            for(int i = 0; i < 3; i++) {
                vector<string>::iterator curr = find(
                    playerData[player].tile.begin(),
                    playerData[player].tile.end(),
                    lastTile);
                if(curr == playerData[player].tile.end()) {
                    playerError(player, "WA");
                }
                playerData[player].tile.erase(curr);
            }
			// 显牌多了四张
            shownTile[lastTile] += 4;
            lastOp = "GANG";
            playerData[player].pack.push_back(PlayerData::Pack("GANG", lastTile, roundStage % 4));
            roundStage = player + 8;
            return true;
        }
        playerError(player, "WA");
    } else if(outputList.size() == 2) {
        if(outputList[0] == "PENG") {
            for(int i = 0; i < 2; i++) {
                vector<string>::iterator curr = find(
                    playerData[player].tile.begin(),
                    playerData[player].tile.end(),
                    lastTile);
                if(curr == playerData[player].tile.end()) {
                    playerError(player, "WA");
                }
                playerData[player].tile.erase(curr);
            }
            shownTile[lastTile] += 3;
            lastOp = "PENG";
            playerData[player].pack.push_back(PlayerData::Pack("PENG", lastTile, roundStage % 4));
            lastTile = outputList[1];
			碰后打出的牌
            vector<string>::iterator curr = find(
                playerData[player].tile.begin(),
                playerData[player].tile.end(),
                lastTile);
            if(curr == playerData[player].tile.end()) {
                playerError(player, "WA");
            }
            playerData[player].tile.erase(curr);
            roundStage = 4 + player;
            return true;
        }
        playerError(player, "WA");
    }
    if(outputList.size() != 3) {
        playerError(player, "WA");
    }
    return false;
}
//3检查吃牌
bool checkInputPLAY3(const Json::Value &playerOutput, int player)
{
    vector<string> outputList;
    string outputString = playerOutput["response"].asString();
    boost::split(outputList, outputString, boost::is_any_of(" "));
    if(outputList.size() == 3) {
		// 吃并且上家
        if(outputList[0] != "CHI" || (roundStage - player) % 4 != 3) {
            playerError(player, "WA");
        }
        playerData[player].tile.push_back(lastTile);
        string c = outputList[1];
        if(str2tile.find(c) == str2tile.end() ||
           (c[0] != 'W' && c[0] != 'B' && c[0] != 'T') ||
           c[0] != lastTile[0] || abs(c[1] - lastTile[1]) > 1) {
            playerError(player, "WA");
        }
        c[1]--;
		// c[1] 现在表示最小的那张
		// 下面循环表示顺子的三张
        for(int i = -1; i <= 1; i++) {
            shownTile[c]++;
            vector<string>::iterator curr = find(
                playerData[player].tile.begin(),
                playerData[player].tile.end(),
                c);
            if(curr == playerData[player].tile.end()) {
                playerError(player, "WA");
            }
            playerData[player].tile.erase(curr);
            c[1]++;
        }
        lastOp = "CHI";
        tileCHI = outputList[1];
        playerData[player].pack.push_back(PlayerData::Pack("CHI", tileCHI, lastTile[1] - outputList[1][1] + 1));
        lastTile = outputList[2];
		// 打出的牌
        vector<string>::iterator curr = find(
            playerData[player].tile.begin(),
            playerData[player].tile.end(),
            lastTile);
        if(curr == playerData[player].tile.end()) {
            playerError(player, "WA");
        }
        playerData[player].tile.erase(curr);
        roundStage = 4 + player;
        return true;
    }
    return false;
}

//检查玩家杠牌后的回应
//用于抢杠和
void checkInputGANG(const Json::Value &playerOutput, int player)
{
    if(playerOutput["verdict"] != "OK") {
        playerError(player, playerOutput["verdict"].asString());
    }
    if(playerOutput["response"] == "PASS") {
        return;
    }
    if(lastBUGANG && roundStage % 4 != player && playerOutput["response"] == "HU") {
		// 抢杠胡
        checkHu(player, true);
    }
    playerError(player, "WA");
}

void roundOutput(Json::Value &outputValue)
{
	// 给玩家的输入
    if(roundStage == -1) {
		// 第二轮，发初始手牌
        outputValue["display"]["action"] = "DEAL";
        for(int i = 0; i < 4; i++) {
			// 先填满再说
            while(playerData[i].tile.size() < 13) {
                string nextTile = *tileWall.rbegin();
                tileWall.pop_back();
                outputValue["display"]["hand"][i].append(nextTile);
                if(nextTile[0] == 'H') {
                    playerData[i].flower.push_back(nextTile);
                } else {
                    playerData[i].tile.push_back(nextTile);
                }
            }
        }
        for(int i = 0; i < 4; i++) {
            string outputString = "1";
            for(int j = 0; j < 4; j++) {
				// 花牌数
                outputString += " " + to_string(playerData[j].flower.size());
            }
            for(int j = 0; j < 13; j++) {
				// 手牌
                outputString += " " + playerData[i].tile[j];
            }
			// 四个人的花牌，按编号排列
            for(int j = 0; j < 4; j++) {
                for(string k : playerData[j].flower)
                    outputString += " " + k;
            }
            outputValue["content"][to_string(i)] = outputString;
        }
    } else if(roundStage >= 0 && roundStage < 4) {
		// 发牌
        if(tileWall.empty()) {
			// 牌完了。。。
            outputValue["display"]["action"] = "HUANG";
            outputValue["command"] = "finish";
            for(int i = 0; i < 4; i++) {
                outputValue["content"][to_string(i)] = 0;
            }
            return;
        }
        lastTile = *tileWall.rbegin();
        tileWall.pop_back();
        if(lastTile[0] == 'H') {
			// 不好意识，给你摸到一张花牌，等我换一下
            lastOp = "BUHUA";
            outputValue["display"]["action"] = "BUHUA";
            outputValue["display"]["player"] = roundStage;
            outputValue["display"]["tile"] = lastTile;
            playerData[roundStage % 4].flower.push_back(lastTile);
            for(int i = 0; i < 4; i++) {
				// 通知：补花啦
                outputValue["content"][to_string(i)] = "3 " + to_string(roundStage) + " BUHUA " + lastTile;
            }
        } else {
            lastOp = "DRAW";
            outputValue["display"]["action"] = "DRAW";
            outputValue["display"]["player"] = roundStage;
            outputValue["display"]["tile"] = lastTile;
            for(int i = 0; i < 4; i++) {
                if(roundStage % 4 == i) {
					// 你摸到了这张牌
                    outputValue["content"][to_string(i)] = "2 " + lastTile;
                } else {
					// 告诉你，这个人摸了一张牌
                    outputValue["content"][to_string(i)] = "3 " + to_string(roundStage % 4) + " DRAW";
                }
            }
			// 能不能胡呢？
            outputValue["display"]["canHu"][roundStage] = checkHu(roundStage, false);
        }
    } else if(roundStage >= 4 && roundStage < 8) {
        outputValue["display"]["action"] = lastOp;
        outputValue["display"]["player"] = roundStage % 4;
        outputValue["display"]["tile"] = lastTile;
        if(lastOp == "CHI") {
			// 吃的那张中间牌
            outputValue["display"]["tileCHI"] = tileCHI;
        }
        for(int i = 0; i < 4; i++) {
            if(lastOp == "CHI") {
				// 有人吃牌并打出一张牌
                outputValue["content"][to_string(i)] = "3 " + to_string(roundStage % 4) + " CHI " + tileCHI + " " + lastTile;
            } else {
				// 有人碰并打出一张牌，或直接打出一张牌
                outputValue["content"][to_string(i)] = "3 " + to_string(roundStage % 4) + " " + lastOp + " " + lastTile;
            }
            outputValue["display"]["canHu"][i] = checkHu(i, false);
        }
    } else {
        string cOp = "GANG";
        if(lastBUGANG) {
            cOp = "BUGANG " + lastTile;
            for(int i = 0; i < 4; i++) {
                if(roundStage % 4 != i) {
                    outputValue["display"]["canHu"][i] = checkHu(i, false);
                }
            }
        }
        outputValue["display"]["action"] = lastBUGANG ? "BUGANG" : "GANG";
        outputValue["display"]["player"] = roundStage % 4;
        outputValue["display"]["tile"] = lastTile;
        for(int i = 0; i < 4; i++) {
            outputValue["content"][to_string(i)] = "3 " + to_string(roundStage % 4) + " " + cOp;
        }
    }
}
void roundInput(Json::Value &inputValue)
{
	处理玩家的输出
    if(roundStage < 0) {
        for(int i = 0; i < 4; i++) {
            checkInputPASS(inputValue[to_string(i)], i);
        }
        roundStage++;
    } else if(roundStage >= 0 && roundStage < 4) {
		// 玩家摸牌
        for(int i = 0; i < 4; i++) {
            if(lastOp == "BUHUA" || roundStage != i) {
				// 上一轮补花或者不是自己摸牌，应输出pass
                checkInputPASS(inputValue[to_string(i)], i);
            } else {
				// 自己摸牌
                checkInputDRAW(inputValue[to_string(i)], i);
            }
        }
        lastBUGANG = currBUGANG;
        currBUGANG = false;
    } else if(roundStage >= 4 && roundStage < 8) {
		// 某人打出牌后
        for(int i = 0; i < 4; i++) {
            if(roundStage == i + 4) {
				// 自己打出牌，应输出pass
                checkInputPASS(inputValue[to_string(i)], i);
            } else {
				// 别人打牌
                checkInputPLAY1(inputValue[to_string(i)], i);
            }
        }
        bool pass = true;
        for(int i = 0; i < 4; i++) {
            if(pass && roundStage != i + 4) {
				// 检查碰、杠
                pass = !checkInputPLAY2(inputValue[to_string(i)], i);
            }
        }
        for(int i = 0; i < 4; i++) {
			// 检查吃牌
            if(pass && roundStage != i + 4) {
                pass = !checkInputPLAY3(inputValue[to_string(i)], i);
            }
        }
        if(pass) {
			// pass为true,意味着其他人都pass，没有碰、吃等操作，则下一轮轮到下一个人摸牌
			// 
            roundStage = (roundStage + 1) % 4;
            shownTile[lastTile]++;
        }
    } else {
		// 玩家杠牌
        for(int i = 0; i < 4; i++) {
            checkInputGANG(inputValue[to_string(i)], i);
        }
		// 轮到摸牌
        roundStage -= 8;
    }
}

int main()
{
#if 0
    freopen("in.txt", "r", stdin);
#endif
    cin >> inputValue;
	// 给裁判的输入
    for(int i = 1; i <= 9; i++) {
        str2tile["W" + to_string(i)] = mahjong::make_tile(TILE_SUIT_CHARACTERS, i);
        str2tile["B" + to_string(i)] = mahjong::make_tile(TILE_SUIT_DOTS, i);
        str2tile["T" + to_string(i)] = mahjong::make_tile(TILE_SUIT_BAMBOO, i);
    }
    for(int i = 1; i <= 4; i++) {
        str2tile["F" + to_string((i))] = mahjong::make_tile(TILE_SUIT_HONORS, i);
    }
    for(int i = 1; i <= 3; i++) {
        str2tile["J" + to_string((i))] = mahjong::make_tile(TILE_SUIT_HONORS, i + 4);
    }
    for(int i = 0; i < 4; i++) {
        outputValue["display"]["canHu"][i] = -4;
    }
    for(int i = 1; i <= 9; i++) {
        for(int j = 0; j < 4; j++) {
            tileWall.push_back("W" + to_string(i));
            tileWall.push_back("B" + to_string(i));
            tileWall.push_back("T" + to_string(i));
        }
    }
    for(int i = 1; i <= 4; i++) {
        for(int j = 0; j < 4; j++) {
            tileWall.push_back("F" + to_string(i));
        }
    }
    for(int i = 1; i <= 3; i++) {
        for(int j = 0; j < 4; j++) {
            tileWall.push_back("J" + to_string(i));
        }
    }
    for(int i = 1; i <= 8; i++) {
        tileWall.push_back("H" + to_string(i));
    }
    unsigned int randSeed = time(nullptr);
    outputValue["command"] = "request";

    // "content": {
    //             "0": "1 1 0 1 2 W3 T5 T1 T7 T7 J1 T2 F2 T4 W6 F2 B6 T5 H3 H1 H7 H5",
    //             "1": "1 1 0 1 2 T9 W9 J1 B3 W7 W7 T3 B3 T9 W1 T7 W8 T5 H3 H1 H7 H5",
    //             "2": "1 1 0 1 2 W3 T2 B5 F1 W2 W7 F3 B7 F3 W4 B5 T3 W8 H3 H1 H7 H5",
    //             "3": "1 1 0 1 2 B4 W3 B7 T4 W5 B1 W5 B3 J2 T3 F1 F4 W9 H3 H1 H7 H5"
    //         }

    string str = "";
    if (inputValue["initdata"].isObject()) {
        if(inputValue["initdata"]["srand"].isUInt()) {
            randSeed = inputValue["initdata"]["srand"].asUInt();
        }
        srand(randSeed);

        if(inputValue["initdata"]["quan"].isUInt()) {
            quan = inputValue["initdata"]["quan"].asUInt();
        }
        else {
            quan=rand()%4;
        }

        if (inputValue["initdata"]["walltiles"].isString())
        {
            str = inputValue["initdata"]["walltiles"].asString();
            if (str.length() != 0) {
                tileWall.clear();
                boost::split(tileWall, str, boost::is_any_of(" "));
            }
        }
    }
    else {
        srand(randSeed);
        quan=rand()%4;
    }

    outputValue["display"]["strlen"] = str.length();
    if (str.length() == 0) {
        random_shuffle(tileWall.begin(), tileWall.end());
        str = "";
        for (auto & x : tileWall) {
            str = str + x + " ";
        }
		// 去掉最后空格
        str.pop_back();
    }
    if(inputValue["log"].size() == 0) {
        outputValue["display"]["action"] = "INIT";
        outputValue["display"]["srand"] = randSeed;
        outputValue["display"]["quan"] = quan;
        // outputValue["display"]["walltiles"] = str;
        for(int i = 0; i < 4; i++) {
			// 第一轮给玩家的输入
            outputValue["content"][to_string(i)] = "0 " + to_string(i) + " " + to_string(quan);
        }
        outputValue["initdata"]["walltiles"] = str;
        outputValue["initdata"]["quan"] = quan;
        outputValue["initdata"]["srand"] = randSeed;
        cout << outputValue;
        return 0;
    }

    roundInput(inputValue["log"][1]);
    for(unsigned int i = 2; i < inputValue["log"].size(); i += 2) {
        //carr << i / 2 << endl;
        Json::Value tmp = outputValue;
        outputValue["display"].clear();
        for(int i = 0; i < 4; i++) {
            outputValue["display"]["canHu"][i] = -4;
        }
		// 给玩家的历史输入
        roundOutput(tmp);
        //carr << inputValue["log"][i + 1] << endl;
		// 处理玩家的输出
        roundInput(inputValue["log"][i + 1]);
    }
    outputValue["display"].clear();
    for(int i = 0; i < 4; i++) {
        outputValue["display"]["canHu"][i] = -4;
    }
	// 当前回合给玩家的输入，需要cout
    roundOutput(outputValue);
    outputValue["display"]["tileCnt"] = tileWall.size();
    cout << outputValue;
    return 0;
}

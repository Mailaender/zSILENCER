#include "terminal.h"
#include "player.h"

Terminal::Terminal() : Object(ObjectTypes::TERMINAL){
	state = INACTIVE;
	//state = READY;
	state_i = 0;
	res_bank = 0xFF;
	res_index = 0;
	inactiveframes = 0;
	beamingframes = 0;
	readyframes = 0;
	hackerid = 0;
	renderpass = 1;
	beamingseconds = 0;
	beamingcount = 0;
	beamingtime = 0;
	requiresauthority = true;
	//SetSize(0);
	isbig = false;
	sizeset = false;
	snapshotinterval = 24;
	tracetime = 0;
	soundchannel = -1;
	secretreadynotified = false;
}

void Terminal::Serialize(bool write, Serializer & data, Serializer * old){
	Object::Serialize(write, data, old);
	data.Serialize(write, state, old);
	data.Serialize(write, state_i, old);
	data.Serialize(write, hackerid, old);
	data.Serialize(write, beamingcount, old);
	data.Serialize(write, beamingseconds, old);
	data.Serialize(write, beamingtime, old);
	data.Serialize(write, tracetime, old);
	data.Serialize(write, isbig, old);
}

void Terminal::Tick(World & world){
	if(!sizeset){
		SetSize(isbig);
	}
	if(state == READY || state == HACKING || state == HACKERGONE){
		if(soundchannel == -1){
			soundchannel = EmitSound(world, world.resources.soundbank["ambloop4.wav"], isbig ? 45 : 32, true);
		}
	}else{
		if(soundchannel != -1){
			Audio::GetInstance().Stop(soundchannel, 1000);
			soundchannel = -1;
		}
	}
	if(hackerid){
		Player * player = (Player *)world.GetObjectFromId(hackerid);
		if(player){
			if(player->state != Player::HACKING){
				hackerid = 0;
				state = HACKERGONE;
			}
		}
	}
	switch(state){
		case INACTIVE:{
			beamingcount = 0;
			res_index = (state_i / 4) % inactiveframes;
		}break;
		case SECRETREADY:
			if(!secretreadynotified){
				world.SendSound("typerev6.wav");
				for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
					if((*it)->type == ObjectTypes::TEAM){
						Team * team = static_cast<Team *>(*it);
						if(team->beamingterminalid == id){
							for(int i = 0; i < team->numpeers; i++){
								Peer * peer = world.peerlist[team->peers[i]];
								if(peer){
									if(!world.intutorialmode){
										char text[256];
										sprintf(text, "TOP SECRET AVAILABLE\n\nGovernment will be able to trace in %d seconds", tracetime);
										world.ShowMessage(text, 255, 0, true, peer);
									}
								}
							}
						}
					}
				}
				secretreadynotified = true;
			}
		case READY:{
			if(state_i >= (inactiveframes + beamingframes + readyframes) * 4){
				state_i = (beamingframes + inactiveframes) * 4;
			}
			res_index = state_i / 4;
		}break;
		case HACKING:
		case HACKERGONE:{
			res_index = ((state_i / 4) % readyframes) + (inactiveframes + beamingframes);
			if(state_i >= juice){
				beamingcount = 0;
				state = INACTIVE;
				state_i = -1;
				break;
			}
		}break;
		case BEAMING:{
			secretreadynotified = false;
			if(world.IsAuthority()){
				if(state_i % 24 == 0){
					beamingcount++;
					if(beamingcount >= beamingseconds){
						state = READY;
						state_i = (beamingframes + inactiveframes) * 4;
						break;
					}
				}
				/*if(state_i >= 240){
					state = READY;
				}*/
			}
			if(beamingframes){
				res_index =  ((state_i / 4) % beamingframes) + (inactiveframes);
			}else{
				res_index = (state_i / 4) % inactiveframes;
			}
		}break;
		case SECRETBEAMING:{
			res_index =  ((state_i) % beamingframes) + (inactiveframes);
		}break;
	}
	if(state != HACKING){
		hackerid = 0;
		state_i++;
	}
	if(tracetime > 0 && world.tickcount % 24 == 0 && world.IsAuthority()){
		tracetime--;
		if(tracetime == 0){
			for(std::list<Object *>::iterator it = world.objectlist.begin(); it != world.objectlist.end(); it++){
				Object * object = *it;
				if(object->type == ObjectTypes::TEAM){
					Team * team = static_cast<Team *>(object);
					if(team->beamingterminalid == id){
						team->beamingterminalid = 0;
					}
				}
			}
			state = INACTIVE;
			state_i = 0;
		}
	}
	if(beamingtime > 0 && world.tickcount % 24 == 0 && world.IsAuthority()){
		beamingtime--;
		if(beamingtime == 0){
			state = SECRETREADY;
			if(!world.intutorialmode){
				tracetime = 90;
				for(std::vector<Uint16>::iterator it = world.objectsbytype[ObjectTypes::TEAM].begin(); it != world.objectsbytype[ObjectTypes::TEAM].end(); it++){
					Team * team = static_cast<Team *>(world.GetObjectFromId(*it));
					if(team && team->beamingterminalid == id){
						switch(team->secrets){
							case 0:
								tracetime = 150;
							break;
							case 1:
								tracetime = 120;
							break;
							case 2:
								tracetime = 90;
							break;
						}
						break;
					}
				}
			}
		}
	}
}

bool Terminal::Hack(Uint16 hackerid){
	if(Terminal::hackerid == 0 || Terminal::hackerid == hackerid){
		if(state == READY || state == HACKERGONE){
			if(state == READY){
				state_i = 0;
			}
			state = HACKING;
			Terminal::hackerid = hackerid;
		}
		if(state == HACKING){
			state_i++;
		}
		return true;
	}
	return false;
}

void Terminal::HackerGone(void){
	if(state == HACKING){
		state = HACKERGONE;
	}
}

Uint8 Terminal::GetPercent(void){
	if(state == READY){
		return 0;
	}
	return Uint8(((float)state_i / juice) * 100);
}

void Terminal::SetSize(bool big){
	isbig = big;
	if(isbig){
		res_bank = 184;
		res_index = 0;
		juice = 150;
		files = 400;
		secretinfo = 45;
		inactiveframes = 5;
		beamingframes = 9;
		readyframes = 9;
		//beamingseconds = 30;
	}else{
		res_bank = 183;
		res_index = 0;
		juice = 80;
		files = 100;
		secretinfo = 15;
		inactiveframes = 5;
		beamingframes = 0;
		readyframes = 5;
		//beamingseconds = 3;
	}
}